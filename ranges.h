/*
 * RinOS C++ <ranges> ?
 * ??????? (C++20)
 */

#ifndef RINCXX_RANGES_H
#define RINCXX_RANGES_H

#include "rincxx.h"
#include "version.h"

#if __cplusplus >= 202002L
#include "algorithm.h"
#include "iterator.h"
#include "memory.h"
#include "type_traits.h"
#include "concepts.h"
#include "utility.h"
#include "functional.h"
#include "compare.h"
#include "tuple.h"

namespace std {

namespace detail {
template<typename Container, typename R>
constexpr Container make_container_from_range(R&& range);

template<typename Container, typename R, typename Tuple, size_t... Is>
constexpr Container make_container_from_range_args(
    R&& range, Tuple&& args, index_sequence<Is...>);
}

#if __cplusplus > 202002L && !defined(RINCXX_FROM_RANGE_T_DEFINED)
#define RINCXX_FROM_RANGE_T_DEFINED
struct from_range_t {
    explicit from_range_t() = default;
};

inline constexpr from_range_t from_range{};
#endif

namespace ranges {

/* ???????????????????????????????????????????????????????????????
 * iterator_t / sentinel_t / range_value_t
 * ???????????????????????????????????????????????????????????????*/

namespace detail {
    void begin() = delete;
    void end() = delete;
    void size() = delete;
    void rbegin() = delete;
    void rend() = delete;

    template<typename T>
    constexpr auto adl_begin(T&& value)
        noexcept(noexcept(begin(std::forward<T>(value))))
        -> decltype(begin(std::forward<T>(value))) {
        return begin(std::forward<T>(value));
    }

    template<typename T>
    constexpr auto adl_end(T&& value)
        noexcept(noexcept(end(std::forward<T>(value))))
        -> decltype(end(std::forward<T>(value))) {
        return end(std::forward<T>(value));
    }

    template<typename T>
    constexpr auto adl_size(T&& value)
        noexcept(noexcept(size(std::forward<T>(value))))
        -> decltype(size(std::forward<T>(value))) {
        return size(std::forward<T>(value));
    }

    template<typename T>
    constexpr auto adl_rbegin(T&& value)
        noexcept(noexcept(rbegin(std::forward<T>(value))))
        -> decltype(rbegin(std::forward<T>(value))) {
        return rbegin(std::forward<T>(value));
    }

    template<typename T>
    constexpr auto adl_rend(T&& value)
        noexcept(noexcept(rend(std::forward<T>(value))))
        -> decltype(rend(std::forward<T>(value))) {
        return rend(std::forward<T>(value));
    }


    template<typename T, typename = void>
    struct has_begin : false_type {};

    template<typename T>
    struct has_begin<T, void_t<decltype(declval<T&>().begin())>> : true_type {};

    template<typename T, typename = void>
    struct has_end : false_type {};

    template<typename T>
    struct has_end<T, void_t<decltype(declval<T&>().end())>> : true_type {};

    template<typename T, typename = void>
    struct has_adl_begin : false_type {};

    template<typename T>
    struct has_adl_begin<T, void_t<decltype(adl_begin(declval<T&>()))>>
        : true_type {};

    template<typename T, typename = void>
    struct has_adl_end : false_type {};

    template<typename T>
    struct has_adl_end<T, void_t<decltype(adl_end(declval<T&>()))>>
        : true_type {};

    template<typename T, typename = void>
    struct has_empty : false_type {};

    template<typename T>
    struct has_empty<T, void_t<decltype(static_cast<bool>(declval<T&>().empty()))>>
        : true_type {};

    template<typename T, typename = void>
    struct has_size : false_type {};

    template<typename T>
    struct has_size<T, void_t<decltype(declval<T&>().size())>> : true_type {};

    template<typename T, typename = void>
    struct has_adl_size : false_type {};

    template<typename T>
    struct has_adl_size<T, void_t<decltype(adl_size(declval<T&>()))>>
        : true_type {};

    template<typename T, typename = void>
    struct has_data : false_type {};

    template<typename T>
    struct has_data<T, void_t<decltype(declval<T&>().data())>> : true_type {};

    template<typename T, typename = void>
    struct has_rbegin : false_type {};

    template<typename T>
    struct has_rbegin<T, void_t<decltype(declval<T&>().rbegin())>>
        : true_type {};

    template<typename T, typename = void>
    struct has_rend : false_type {};

    template<typename T>
    struct has_rend<T, void_t<decltype(declval<T&>().rend())>> : true_type {};

    template<typename T, typename = void>
    struct has_adl_rbegin : false_type {};

    template<typename T>
    struct has_adl_rbegin<T, void_t<decltype(adl_rbegin(declval<T&>()))>>
        : true_type {};

    template<typename T, typename = void>
    struct has_adl_rend : false_type {};

    template<typename T>
    struct has_adl_rend<T, void_t<decltype(adl_rend(declval<T&>()))>>
        : true_type {};


    template<typename R, bool = has_begin<R>::value, typename = void>
    struct range_begin_access {};

    template<typename R>
    struct range_begin_access<R, true, void> {
        using type = decltype(declval<R&>().begin());
    };

    template<typename R>
    struct range_begin_access<
        R, false,
        void_t<decltype(adl_begin(declval<R&>()))>> {
        using type = decltype(adl_begin(declval<R&>()));
    };

    template<typename R, bool = has_end<R>::value, typename = void>
    struct range_end_access {};

    template<typename R>
    struct range_end_access<R, true, void> {
        using type = decltype(declval<R&>().end());
    };

    template<typename R>
    struct range_end_access<
        R, false,
        void_t<decltype(adl_end(declval<R&>()))>> {
        using type = decltype(adl_end(declval<R&>()));
    };

    template<typename R,
             bool = is_array<remove_reference_t<R>>::value,
             typename = void>
    struct range_access {};

    template<typename R>
    struct range_access<
        R, false,
        void_t<typename range_begin_access<R>::type,
               typename range_end_access<R>::type>> {
        using iterator = typename range_begin_access<R>::type;
        using sentinel = typename range_end_access<R>::type;
    };

    template<typename R>
    struct range_access<R, true, void> {
        using iterator = remove_extent_t<remove_reference_t<R>>*;
        using sentinel = iterator;
    };

    template<typename R, typename = void>
    struct has_sized_sentinel : false_type {};

    template<typename R>
    struct has_sized_sentinel<
        R,
        void_t<typename range_access<R>::iterator,
               typename range_access<R>::sentinel,
               decltype(declval<typename range_access<R>::sentinel>() -
                        declval<typename range_access<R>::iterator>()),
               decltype(declval<typename range_access<R>::iterator>() -
                        declval<typename range_access<R>::sentinel>())>>
        : integral_constant<bool,
            sized_sentinel_for<typename range_access<R>::sentinel,
                               typename range_access<R>::iterator>> {};

}

template<typename R>
using iterator_t = typename detail::range_access<R>::iterator;

template<typename R>
using sentinel_t = typename detail::range_access<R>::sentinel;

template<typename R>
using range_value_t = iter_value_t<iterator_t<R>>;

template<typename R>
using range_reference_t = iter_reference_t<iterator_t<R>>;

template<typename R>
using range_difference_t = iter_difference_t<iterator_t<R>>;

/* ???????????????????????????????????????????????????????????????
 * range ????? (C++20 concept)
 * ???????????????????????????????????????????????????????????????*/

template<typename T>
concept range = requires {
    typename detail::range_access<T>::iterator;
    typename detail::range_access<T>::sentinel;
} && input_or_output_iterator<iterator_t<T>>;

template<typename T>
concept input_range = range<T> &&
    input_iterator<iterator_t<T>>;

template<typename T>
concept forward_range = input_range<T> &&
    forward_iterator<iterator_t<T>>;

template<typename T>
concept bidirectional_range = forward_range<T> &&
    bidirectional_iterator<iterator_t<T>>;

template<typename T>
concept random_access_range = bidirectional_range<T> &&
    random_access_iterator<iterator_t<T>>;

template<typename T>
concept common_range = range<T> &&
    same_as<iterator_t<T>, sentinel_t<T>>;

template<typename T>
concept contiguous_range = random_access_range<T> &&
    contiguous_iterator<iterator_t<T>>;

/* A specialization suppresses member and ADL size customizations.  The
 * sized-sentinel end - begin fallback remains independently available. */
template<typename T>
inline constexpr bool disable_sized_range = false;

template<typename T>
concept sized_range = range<T> &&
    ((!disable_sized_range<remove_cvref_t<T>> &&
      (detail::has_size<T>::value || detail::has_adl_size<T>::value)) ||
     detail::has_sized_sentinel<T>::value ||
     is_array<remove_reference_t<T>>::value);

/* borrowed_range - ????????????????? */
template<typename T>
inline constexpr bool enable_borrowed_range = false;

/* ????? borrowed_range */
template<typename T, size_t N>
inline constexpr bool enable_borrowed_range<T[N]> = true;

template<typename T>
concept maybe_borrowed_range =
    is_lvalue_reference_v<T> || enable_borrowed_range<remove_cvref_t<T>>;

template<typename T>
concept borrowed_range = range<T> && maybe_borrowed_range<T>;

/* A range algorithm returns this marker instead of an iterator when a
 * temporary non-borrowed range would make the iterator dangle. */
struct dangling {
    constexpr dangling() noexcept = default;

    template<typename T>
    constexpr dangling(T&&) noexcept {}
};

template<typename R>
using borrowed_iterator_t = conditional_t<borrowed_range<R>, iterator_t<R>,
                                         dangling>;

/* ???????????????????????????????????????????????????????????????
 * begin / end ??????????????
 * ???????????????????????????????????????????????????????????????*/

namespace __cpo {

struct begin_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             detail::has_begin<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).begin()))
        -> decltype(std::forward<R>(r).begin()) {
        return std::forward<R>(r).begin();
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_begin<remove_reference_t<R>>::value &&
                             detail::has_adl_begin<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(detail::adl_begin(std::forward<R>(r))))
        -> decltype(detail::adl_begin(std::forward<R>(r))) {
        return detail::adl_begin(std::forward<R>(r));
    }

    template<typename T, size_t N>
    constexpr T* operator()(T (&arr)[N]) const noexcept {
        return arr;
    }
};

struct end_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             detail::has_end<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).end()))
        -> decltype(std::forward<R>(r).end()) {
        return std::forward<R>(r).end();
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_end<remove_reference_t<R>>::value &&
                             detail::has_adl_end<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(detail::adl_end(std::forward<R>(r))))
        -> decltype(detail::adl_end(std::forward<R>(r))) {
        return detail::adl_end(std::forward<R>(r));
    }

    template<typename T, size_t N>
    constexpr T* operator()(T (&arr)[N]) const noexcept {
        return arr + N;
    }
};

struct size_fn {
    template<typename R,
             enable_if_t<!disable_sized_range<remove_cvref_t<R>> &&
                             detail::has_size<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).size()))
        -> decltype(std::forward<R>(r).size()) {
        return std::forward<R>(r).size();
    }

    template<typename R,
             enable_if_t<!disable_sized_range<remove_cvref_t<R>> &&
                             !detail::has_size<remove_reference_t<R>>::value &&
                             detail::has_adl_size<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(detail::adl_size(std::forward<R>(r))))
        -> decltype(detail::adl_size(std::forward<R>(r))) {
        return detail::adl_size(std::forward<R>(r));
    }

    template<typename R,
             enable_if_t<!detail::has_size<remove_reference_t<R>>::value &&
                             !detail::has_adl_size<remove_reference_t<R>>::value &&
                             !is_array<remove_reference_t<R>>::value &&
                             detail::has_sized_sentinel<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(end_fn{}(std::forward<R>(r)) -
                          begin_fn{}(std::forward<R>(r))))
        -> make_unsigned_t<remove_cv_t<decltype(
            end_fn{}(std::forward<R>(r)) - begin_fn{}(std::forward<R>(r)))>> {
        using result_type = make_unsigned_t<remove_cv_t<decltype(
            end_fn{}(std::forward<R>(r)) - begin_fn{}(std::forward<R>(r)))>>;
        return static_cast<result_type>(
            end_fn{}(std::forward<R>(r)) - begin_fn{}(std::forward<R>(r)));
    }

    template<typename T, size_t N>
    constexpr size_t operator()(T (&)[N]) const noexcept {
        return N;
    }
};

struct empty_fn {
private:
    template<typename R>
    static constexpr bool is_noexcept()
    {
        using range_type = remove_reference_t<R>;
        if constexpr (detail::has_empty<range_type>::value) {
            return noexcept(static_cast<bool>(
                declval<R&&>().empty()));
        } else if constexpr ((!disable_sized_range<remove_cvref_t<R>> &&
                              (detail::has_size<range_type>::value ||
                               detail::has_adl_size<range_type>::value)) ||
                             detail::has_sized_sentinel<range_type>::value ||
                             is_array<range_type>::value) {
            return noexcept(size_fn{}(declval<R&&>()) == 0);
        } else {
            return noexcept(begin_fn{}(declval<R&>()) ==
                            end_fn{}(declval<R&>()));
        }
    }

public:
    template<typename R>
    constexpr bool operator()(R&& r) const noexcept(is_noexcept<R>()) {
        using range_type = remove_reference_t<R>;
        if constexpr (detail::has_empty<range_type>::value) {
            return static_cast<bool>(std::forward<R>(r).empty());
        } else if constexpr ((!disable_sized_range<remove_cvref_t<R>> &&
                              (detail::has_size<range_type>::value ||
                               detail::has_adl_size<range_type>::value)) ||
                             detail::has_sized_sentinel<range_type>::value ||
                             is_array<range_type>::value) {
            return size_fn{}(std::forward<R>(r)) == 0;
        } else {
            return begin_fn{}(r) == end_fn{}(r);
        }
    }
};

struct data_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             detail::has_data<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).data()))
        -> decltype(std::forward<R>(r).data()) {
        return std::forward<R>(r).data();
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_data<remove_reference_t<R>>::value &&
                             is_pointer<decltype(begin_fn{}(declval<R&>()))>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(begin_fn{}(std::forward<R>(r))))
        -> decltype(begin_fn{}(std::forward<R>(r))) {
        return begin_fn{}(std::forward<R>(r));
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_data<remove_reference_t<R>>::value &&
                             !is_pointer<decltype(begin_fn{}(declval<R&>()))>::value &&
                             contiguous_iterator<decltype(
                                 begin_fn{}(declval<R&>()))>,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::to_address(
            begin_fn{}(std::forward<R>(r)))))
        -> decltype(std::to_address(
            begin_fn{}(std::forward<R>(r)))) {
        return std::to_address(begin_fn{}(std::forward<R>(r)));
    }

    template<typename T, size_t N>
    constexpr T* operator()(T (&arr)[N]) const noexcept {
        return arr;
    }
};

struct cbegin_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(begin_fn{}(std::as_const(r))))
        -> decltype(begin_fn{}(std::as_const(r))) {
        return begin_fn{}(std::as_const(r));
    }
};

struct cend_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(end_fn{}(std::as_const(r))))
        -> decltype(end_fn{}(std::as_const(r))) {
        return end_fn{}(std::as_const(r));
    }
};

struct rbegin_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             detail::has_rbegin<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).rbegin()))
        -> decltype(std::forward<R>(r).rbegin()) {
        return std::forward<R>(r).rbegin();
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_rbegin<remove_reference_t<R>>::value &&
                             detail::has_adl_rbegin<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(detail::adl_rbegin(std::forward<R>(r))))
        -> decltype(detail::adl_rbegin(std::forward<R>(r))) {
        return detail::adl_rbegin(std::forward<R>(r));
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_rbegin<remove_reference_t<R>>::value &&
                             !detail::has_adl_rbegin<remove_reference_t<R>>::value &&
                             !is_array<remove_reference_t<R>>::value &&
                             is_same<decltype(begin_fn{}(declval<R&>())),
                                     decltype(end_fn{}(declval<R&>()))>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(make_reverse_iterator(
            end_fn{}(std::forward<R>(r)))))
        -> decltype(make_reverse_iterator(end_fn{}(std::forward<R>(r)))) {
        return make_reverse_iterator(end_fn{}(std::forward<R>(r)));
    }

    template<typename T, size_t N>
    constexpr auto operator()(T (&array)[N]) const noexcept {
        return make_reverse_iterator(array + N);
    }
};

struct rend_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             detail::has_rend<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(std::forward<R>(r).rend()))
        -> decltype(std::forward<R>(r).rend()) {
        return std::forward<R>(r).rend();
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_rend<remove_reference_t<R>>::value &&
                             detail::has_adl_rend<remove_reference_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(detail::adl_rend(std::forward<R>(r))))
        -> decltype(detail::adl_rend(std::forward<R>(r))) {
        return detail::adl_rend(std::forward<R>(r));
    }

    template<typename R,
             enable_if_t<maybe_borrowed_range<R> &&
                             !detail::has_rend<remove_reference_t<R>>::value &&
                             !detail::has_adl_rend<remove_reference_t<R>>::value &&
                             !is_array<remove_reference_t<R>>::value &&
                             is_same<decltype(begin_fn{}(declval<R&>())),
                                     decltype(end_fn{}(declval<R&>()))>::value,
                         int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(make_reverse_iterator(
            begin_fn{}(std::forward<R>(r)))))
        -> decltype(make_reverse_iterator(begin_fn{}(std::forward<R>(r)))) {
        return make_reverse_iterator(begin_fn{}(std::forward<R>(r)));
    }

    template<typename T, size_t N>
    constexpr auto operator()(T (&array)[N]) const noexcept {
        return make_reverse_iterator(array);
    }
};

struct crbegin_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(rbegin_fn{}(std::as_const(r))))
        -> decltype(rbegin_fn{}(std::as_const(r))) {
        return rbegin_fn{}(std::as_const(r));
    }
};

struct crend_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(rend_fn{}(std::as_const(r))))
        -> decltype(rend_fn{}(std::as_const(r))) {
        return rend_fn{}(std::as_const(r));
    }
};

struct cdata_fn {
    template<typename R,
             enable_if_t<maybe_borrowed_range<R>, int> = 0>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(data_fn{}(std::as_const(r))))
        -> decltype(data_fn{}(std::as_const(r))) {
        return data_fn{}(std::as_const(r));
    }
};

template<typename R>
using ssize_result_t = common_type_t<
    ptrdiff_t,
    make_signed_t<remove_cv_t<decltype(size_fn{}(declval<R&>()))>>>;

struct ssize_fn {
    template<typename R>
    constexpr auto operator()(R&& r) const
        noexcept(noexcept(size_fn{}(std::forward<R>(r))))
        -> ssize_result_t<R> {
        return static_cast<ssize_result_t<R>>(
            size_fn{}(std::forward<R>(r)));
    }
};

} /* namespace __cpo */

inline constexpr __cpo::begin_fn begin{};
inline constexpr __cpo::end_fn end{};
inline constexpr __cpo::size_fn size{};
inline constexpr __cpo::empty_fn empty{};
inline constexpr __cpo::data_fn data{};
inline constexpr __cpo::cbegin_fn cbegin{};
inline constexpr __cpo::cend_fn cend{};
inline constexpr __cpo::rbegin_fn rbegin{};
inline constexpr __cpo::rend_fn rend{};
inline constexpr __cpo::crbegin_fn crbegin{};
inline constexpr __cpo::crend_fn crend{};
inline constexpr __cpo::cdata_fn cdata{};
inline constexpr __cpo::ssize_fn ssize{};

/* ???????????????????????????????????????????????????????????????
 * view_base - ??????????
 * ???????????????????????????????????????????????????????????????*/

struct view_base {};

template<typename T>
inline constexpr bool enable_view =
    is_base_of_v<view_base, T> || (range<T> && is_empty<T>::value);

template<typename T>
concept view = range<T> && movable<T> && enable_view<T>;

#if __cplusplus > 202002L
namespace detail {
template<typename T>
struct is_initializer_list : false_type {};

template<typename T>
struct is_initializer_list<initializer_list<T>> : true_type {};
} /* namespace detail */
#endif

template<typename T>
concept viewable_range = range<T> &&
#if __cplusplus > 202002L
    ((view<remove_cvref_t<T>> &&
      constructible_from<remove_cvref_t<T>, T>) ||
     (!view<remove_cvref_t<T>> &&
      (is_lvalue_reference_v<T> ||
       (movable<remove_cvref_t<T>> &&
        !detail::is_initializer_list<remove_cvref_t<T>>::value))));
#else
    (borrowed_range<T> ||
     (view<remove_cvref_t<T>> &&
      constructible_from<remove_cvref_t<T>, T>));
#endif

#if __cplusplus > 202002L
template<typename I>
concept constant_iterator = input_iterator<I> &&
    same_as<iter_reference_t<I>, const iter_value_t<I>&>;

template<typename T>
concept constant_range = input_range<T> &&
    constant_iterator<iterator_t<T>>;
#endif

/* ???????????????????????????????????????????????????????????????
 * view_interface - CRTP??????
 * ???????????????????????????????????????????????????????????????*/

template<typename Derived>
class view_interface : public view_base {
    constexpr Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }
    constexpr const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

public:
    constexpr bool empty() {
        return ranges::begin(derived()) == ranges::end(derived());
    }

    constexpr explicit operator bool() {
        return !empty();
    }

    template<typename R = Derived>
    constexpr auto size() -> decltype(ranges::end(declval<R&>()) - ranges::begin(declval<R&>())) {
        return ranges::end(derived()) - ranges::begin(derived());
    }

    template<typename R = Derived>
    constexpr auto front() -> decltype(*ranges::begin(declval<R&>())) {
        return *ranges::begin(derived());
    }

    template<typename R = Derived>
    constexpr auto back() -> decltype(*--ranges::end(declval<R&>())) {
        auto it = ranges::end(derived());
        --it;
        return *it;
    }

    template<typename R = Derived>
    constexpr auto operator[](range_difference_t<R> n)
        -> decltype(ranges::begin(declval<R&>())[n]) {
        return ranges::begin(derived())[n];
    }
};

/* ???????????????????????????????????????????????????????????????
 * subrange - ???????????
 * ???????????????????????????????????????????????????????????????*/

template<typename I, typename S = I>
class subrange : public view_interface<subrange<I, S>> {
    I begin_;
    S end_;

public:
    subrange() = default;

    constexpr subrange(I begin, S end) : begin_(begin), end_(end) {}

    template<typename R>
    constexpr subrange(R&& r)
        : begin_(ranges::begin(r)), end_(ranges::end(r)) {}

    /* The range customization point returns iterator/sentinel values.  Using
     * references here makes iterator_t<subrange<...>> become I& and causes
     * the iterator concepts to reject otherwise valid pointer iterators. */
    constexpr I begin() & noexcept { return begin_; }
    constexpr I begin() const& noexcept { return begin_; }
    constexpr S end() & noexcept { return end_; }
    constexpr S end() const& noexcept { return end_; }
    constexpr I&& begin() && noexcept { return std::move(begin_); }
    constexpr const I&& begin() const&& noexcept { return std::move(begin_); }
    constexpr S&& end() && noexcept { return std::move(end_); }
    constexpr const S&& end() const&& noexcept { return std::move(end_); }

    constexpr auto size() const requires sized_sentinel_for<S, I> {
        return end_ - begin_;
    }
};

/* ????? */
template<typename I, typename S>
subrange(I, S) -> subrange<I, S>;

template<typename R>
subrange(R&&) -> subrange<iterator_t<R>, sentinel_t<R>>;

/* A subrange stores its iterator/sentinel pair rather than the source range,
 * so it remains valid when the subrange object itself is a temporary. */
template<typename I, typename S>
inline constexpr bool enable_borrowed_range<subrange<I, S>> = true;

/* C++23 exposes subrange as a standard tuple-like source.  Keep get in the
 * associated ranges namespace so dependent tuple construction finds it by
 * ADL even when <tuple> was included before <ranges>. */
template<size_t Index, typename I, typename S>
constexpr decltype(auto) get(subrange<I, S>& value) noexcept {
    static_assert(Index < 2, "subrange get index out of bounds");
    if constexpr (Index == 0) return (value.begin());
    else return (value.end());
}

template<size_t Index, typename I, typename S>
constexpr decltype(auto) get(const subrange<I, S>& value) noexcept {
    static_assert(Index < 2, "subrange get index out of bounds");
    if constexpr (Index == 0) return (value.begin());
    else return (value.end());
}

template<size_t Index, typename I, typename S>
constexpr decltype(auto) get(subrange<I, S>&& value) noexcept {
    static_assert(Index < 2, "subrange get index out of bounds");
    if constexpr (Index == 0) return std::move(value).begin();
    else return std::move(value).end();
}

template<size_t Index, typename I, typename S>
constexpr decltype(auto) get(const subrange<I, S>&& value) noexcept {
    static_assert(Index < 2, "subrange get index out of bounds");
    if constexpr (Index == 0) return std::move(value).begin();
    else return std::move(value).end();
}

/* ???????????????????????????????????????????????????????????????
 * ref_view - ??????
 * ???????????????????????????????????????????????????????????????*/

template<typename R>
class ref_view : public view_interface<ref_view<R>> {
    R* range_;

public:
    constexpr ref_view(R& r) noexcept : range_(&r) {}

    constexpr R& base() const { return *range_; }
    constexpr iterator_t<R> begin() { return ranges::begin(*range_); }
    constexpr auto begin() const requires range<const R> {
        return ranges::begin(std::as_const(*range_));
    }
    constexpr sentinel_t<R> end() { return ranges::end(*range_); }
    constexpr auto end() const requires range<const R> {
        return ranges::end(std::as_const(*range_));
    }

    constexpr auto size() const requires sized_range<const R> {
        return ranges::size(std::as_const(*range_));
    }
    constexpr auto data() const requires contiguous_range<const R> {
        return ranges::data(std::as_const(*range_));
    }
};

template<typename R>
ref_view(R&) -> ref_view<R>;
/* A ref_view never owns its range, so iterators remain valid for the
 * lifetime of the referenced object even when the view itself is a temporary. */
template<typename R>
inline constexpr bool enable_borrowed_range<ref_view<R>> = true;

/* ???????????????????????????????????????????????????????????????
 * owning_view - ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename R>
class owning_view : public view_interface<owning_view<R>> {
    R range_;

public:
    owning_view() = default;
    constexpr owning_view(R&& r) : range_(std::move(r)) {}

    owning_view(const owning_view&) = default;
    owning_view(owning_view&&) = default;
    owning_view& operator=(const owning_view&) = default;
    owning_view& operator=(owning_view&&) = default;

    constexpr R& base() & noexcept { return range_; }
    constexpr const R& base() const& noexcept { return range_; }
    constexpr R&& base() && noexcept { return std::move(range_); }

    constexpr iterator_t<R> begin() { return ranges::begin(range_); }
    constexpr sentinel_t<R> end() { return ranges::end(range_); }
      constexpr auto begin() const requires range<const R> {
          return ranges::begin(std::as_const(range_));
      }
      constexpr auto end() const requires range<const R> {
          return ranges::end(std::as_const(range_));
      }
      constexpr auto size() requires sized_range<R> {
          return ranges::size(range_);
      }
      constexpr auto size() const requires sized_range<const R> {
          return ranges::size(std::as_const(range_));
      }
};

template<typename R>
owning_view(R&&) -> owning_view<R>;

/* ???????????????????????????????????????????????????????????????
 * iota_view - ????
 * ???????????????????????????????????????????????????????????????*/

namespace detail {

template<typename W>
concept iota_value = copy_constructible<W> && equality_comparable<W> &&
    requires(W value) {
        { ++value } -> same_as<W&>;
        value++;
    };

template<typename W>
concept iota_decrementable = requires(W value) {
    { --value } -> same_as<W&>;
    value--;
};

template<typename W>
concept iota_random_access = iota_decrementable<W> &&
    requires(W value, ptrdiff_t offset) {
        { value += offset } -> same_as<W&>;
        { value -= offset } -> same_as<W&>;
        value + offset;
        value - offset;
        value - value;
    };

template<typename W, typename Bound>
concept iota_bounded = copy_constructible<Bound> &&
    requires(const W& value, const Bound& bound) {
        static_cast<bool>(value == bound);
        static_cast<bool>(value < bound);
        static_cast<size_t>(bound - value);
    };

template<typename W, typename Bound>
constexpr size_t iota_distance(W first, Bound last) {
    if constexpr (integral<W> && integral<Bound>) {
        using common = common_type_t<W, Bound>;
        using unsigned_common = make_unsigned_t<common>;
        const unsigned_common delta =
            static_cast<unsigned_common>(last) -
            static_cast<unsigned_common>(first);
        if constexpr (sizeof(unsigned_common) > sizeof(size_t)) {
            const unsigned_common maximum =
                static_cast<unsigned_common>(static_cast<size_t>(-1));
            if (delta > maximum) return static_cast<size_t>(-1);
        }
        return static_cast<size_t>(delta);
    } else {
        return static_cast<size_t>(last - first);
    }
}

} /* namespace detail */

template<typename W, typename Bound = unreachable_sentinel_t>
    requires detail::iota_value<W> &&
             (same_as<Bound, unreachable_sentinel_t> ||
              detail::iota_bounded<W, Bound>)
class iota_view : public view_interface<iota_view<W, Bound>> {
    W value_;
    Bound bound_;

    struct iterator {
        using iterator_category = conditional_t<
            detail::iota_random_access<W>, random_access_iterator_tag,
            conditional_t<detail::iota_decrementable<W>,
                          bidirectional_iterator_tag,
                          forward_iterator_tag>>;
        using iterator_concept = iterator_category;
        using value_type = W;
        using difference_type = ptrdiff_t;
        using pointer = const W*;
        using reference = W;

        W value_{};

        constexpr iterator() = default;
        constexpr explicit iterator(W value) : value_(std::move(value)) {}

        constexpr W operator*() const { return value_; }
        constexpr iterator& operator++() { ++value_; return *this; }
        constexpr iterator operator++(int) { auto copy = *this; ++*this; return copy; }

        constexpr iterator& operator--()
            requires detail::iota_decrementable<W> {
            --value_; return *this;
        }
        constexpr iterator operator--(int)
            requires detail::iota_decrementable<W> {
            auto copy = *this; --*this; return copy;
        }

        constexpr iterator& operator+=(difference_type offset)
            requires detail::iota_random_access<W> {
            value_ += offset; return *this;
        }
        constexpr iterator& operator-=(difference_type offset)
            requires detail::iota_random_access<W> {
            value_ -= offset; return *this;
        }
        constexpr W operator[](difference_type offset) const
            requires detail::iota_random_access<W> {
            return value_ + offset;
        }

        friend constexpr bool operator==(iterator left, iterator right)
        { return left.value_ == right.value_; }
        friend constexpr bool operator!=(iterator left, iterator right)
        { return !(left == right); }

        friend constexpr bool operator<(iterator left, iterator right)
            requires detail::iota_random_access<W> { return left.value_ < right.value_; }
        friend constexpr bool operator>(iterator left, iterator right)
            requires detail::iota_random_access<W> { return right < left; }
        friend constexpr bool operator<=(iterator left, iterator right)
            requires detail::iota_random_access<W> { return !(right < left); }
        friend constexpr bool operator>=(iterator left, iterator right)
            requires detail::iota_random_access<W> { return !(left < right); }
        friend constexpr iterator operator+(iterator value, difference_type offset)
            requires detail::iota_random_access<W> { value += offset; return value; }
        friend constexpr iterator operator+(difference_type offset, iterator value)
            requires detail::iota_random_access<W> { value += offset; return value; }
        friend constexpr iterator operator-(iterator value, difference_type offset)
            requires detail::iota_random_access<W> { value -= offset; return value; }
        friend constexpr difference_type operator-(iterator left, iterator right)
            requires detail::iota_random_access<W> {
            if constexpr (integral<W>) {
                if (right.value_ <= left.value_)
                    return static_cast<difference_type>(
                        detail::iota_distance(right.value_, left.value_));
                return -static_cast<difference_type>(
                    detail::iota_distance(left.value_, right.value_));
            } else {
                return static_cast<difference_type>(left.value_ - right.value_);
            }
        }
    };

    struct bound_sentinel {
        Bound bound_{};
        W start_{};
        bool empty_ = false;

        constexpr bound_sentinel() = default;
        constexpr bound_sentinel(Bound bound, W start, bool empty)
            : bound_(std::move(bound)), start_(std::move(start)), empty_(empty) {}

        friend constexpr bool operator==(iterator value, bound_sentinel bound)
        { return value.value_ == (bound.empty_ ? bound.start_ : bound.bound_); }
        friend constexpr bool operator!=(iterator value, bound_sentinel bound)
        { return !(value == bound); }
        friend constexpr bool operator==(bound_sentinel bound, iterator value)
        { return value == bound; }
        friend constexpr bool operator!=(bound_sentinel bound, iterator value)
        { return !(value == bound); }
    };

public:
    iota_view() = default;
    constexpr explicit iota_view(W value)
        requires same_as<Bound, unreachable_sentinel_t>
        : value_(std::move(value)), bound_() {}
    constexpr iota_view(W value, Bound bound)
        : value_(std::move(value)), bound_(std::move(bound)) {}

    constexpr iterator begin() const { return iterator(value_); }

    constexpr bool empty() const
        requires (!same_as<Bound, unreachable_sentinel_t>) {
        return !(value_ < bound_);
    }
    constexpr bool empty() const
        requires same_as<Bound, unreachable_sentinel_t> { return false; }

    constexpr auto end() const {
        if constexpr (same_as<Bound, unreachable_sentinel_t>) {
            return unreachable_sentinel;
        } else if constexpr (same_as<W, Bound>) {
            return iterator(empty() ? value_ : bound_);
        } else {
            return bound_sentinel(bound_, value_, empty());
        }
    }

    constexpr size_t size() const
        requires (!same_as<Bound, unreachable_sentinel_t>) {
        if (empty()) return 0;
        return detail::iota_distance(value_, bound_);
    }
};

template<typename W, typename Bound>
iota_view(W, Bound) -> iota_view<W, Bound>;

template<typename W>
iota_view(W) -> iota_view<W, unreachable_sentinel_t>;
/* ???????????????????????????????????????????????????????????????
 * empty_view - ????
 * ???????????????????????????????????????????????????????????????*/

template<typename T>
class empty_view : public view_interface<empty_view<T>> {
public:
    static constexpr T* begin() noexcept { return nullptr; }
    static constexpr T* end() noexcept { return nullptr; }
    static constexpr T* data() noexcept { return nullptr; }
    static constexpr size_t size() noexcept { return 0; }
    static constexpr bool empty() noexcept { return true; }
};

/* ???????????????????????????????????????????????????????????????
 * single_view - ???????
 * ???????????????????????????????????????????????????????????????*/

template<typename T>
    requires copy_constructible<T> && is_object_v<T> &&
             same_as<T, remove_cv_t<T>>
class single_view : public view_interface<single_view<T>> {
    T value_;

public:
    single_view() requires default_initializable<T> = default;
    constexpr explicit single_view(const T& value) : value_(value) {}
    constexpr explicit single_view(T&& value) : value_(std::move(value)) {}

    constexpr T* begin() noexcept { return &value_; }
    constexpr const T* begin() const noexcept { return &value_; }
    constexpr T* end() noexcept { return &value_ + 1; }
    constexpr const T* end() const noexcept { return &value_ + 1; }
    static constexpr size_t size() noexcept { return 1; }
    constexpr T* data() noexcept { return &value_; }
    constexpr const T* data() const noexcept { return &value_; }
};

template<typename T>
    requires copy_constructible<T> && is_object_v<T> &&
             same_as<T, remove_cv_t<T>>
single_view(T) -> single_view<T>;

/* ???????????????????????????????????????????????????????????????
 * join_view - ????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
concept join_view_base = input_range<V> && range<remove_reference_t<range_reference_t<V>>>;


template<typename V>
concept join_viewable = viewable_range<V> &&
    (is_lvalue_reference_v<V> || view<remove_cvref_t<V>> ||
     range<const remove_reference_t<V>>) &&
    join_view_base<remove_reference_t<V>>;

#if __cplusplus > 202002L
template<typename V, typename D>
concept join_with_view_base = join_view_base<V> &&
    requires {
        typename common_type_t<
            remove_cvref_t<range_reference_t<
                remove_reference_t<range_reference_t<V>>>>, D>;
        requires common_reference_with<
            range_reference_t<
                remove_reference_t<range_reference_t<V>>>,
            const D&>;
    };

template<typename R, typename D>
concept join_with_factory_viewable = join_viewable<R> &&
    join_with_view_base<remove_reference_t<R>, remove_cvref_t<D>>;
#endif


template<typename V>
    requires join_view_base<V>
class join_view : public view_interface<join_view<V>> {
    template<typename Base>
    struct sentinel_impl {
        using OS = sentinel_t<Base>;
        OS outer_end_{};
        constexpr sentinel_impl() = default;
        constexpr explicit sentinel_impl(OS outer_end)
            : outer_end_(outer_end) {}
    };

    template<typename Base>
    struct iterator_impl {
        using O = iterator_t<Base>;
        using OS = sentinel_t<Base>;
        using OuterReference = range_reference_t<Base>;
        using InnerRange = remove_reference_t<OuterReference>;
        using I = iterator_t<InnerRange>;
        using S = sentinel_t<InnerRange>;
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = range_value_t<InnerRange>;
        using difference_type = range_difference_t<InnerRange>;
        using pointer = void;
        using reference = range_reference_t<InnerRange>;

        O outer_{};
        OS outer_end_{};
        I inner_{};
        S inner_end_{};
        bool at_end_ = true;

        constexpr iterator_impl() = default;

        constexpr iterator_impl(O outer, OS outer_end)
            : outer_(outer), outer_end_(outer_end), inner_(), inner_end_(),
              at_end_(false) {
            satisfy();
        }

        constexpr void satisfy() {
            while (outer_ != outer_end_) {
                auto&& inner_range = *outer_;
                inner_ = ranges::begin(inner_range);
                inner_end_ = ranges::end(inner_range);
                if (inner_ != inner_end_) {
                    at_end_ = false;
                    return;
                }
                ++outer_;
            }
            at_end_ = true;
        }

        constexpr decltype(auto) operator*() const { return *inner_; }

        constexpr iterator_impl& operator++() {
            if (at_end_) return *this;
            ++inner_;
            if (inner_ == inner_end_) {
                ++outer_;
                satisfy();
            }
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& left,
                                         const iterator_impl& right) {
            if (left.at_end_ || right.at_end_)
                return left.at_end_ == right.at_end_;
            return left.outer_ == right.outer_ &&
                   left.inner_ == right.inner_;
        }

        friend constexpr bool operator!=(const iterator_impl& left,
                                         const iterator_impl& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return value.at_end_ || value.outer_ == bound.outer_end_;
        }

        friend constexpr bool operator==(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return !(value == bound);
        }
    };

    using iterator = iterator_impl<V>;
    using sentinel = sentinel_impl<V>;

    V base_;

public:
    join_view() = default;
    constexpr explicit join_view(V base) : base_(std::move(base)) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator(ranges::begin(base_), ranges::end(base_));
    }

    constexpr auto end() {
        return sentinel(ranges::end(base_));
    }

    constexpr auto begin() const
        requires join_view_base<const V> {
        return iterator_impl<const V>(ranges::begin(std::as_const(base_)),
                                      ranges::end(std::as_const(base_)));
    }

    constexpr auto end() const
        requires join_view_base<const V> {
        return sentinel_impl<const V>(ranges::end(std::as_const(base_)));
    }
};

template<typename R>
join_view(R&&) -> join_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<join_view<V>> = borrowed_range<V>;

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * join_with_view - inner range ??? delimiter ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V, typename D>
    requires join_with_view_base<V, D>
class join_with_view : public view_interface<join_with_view<V, D>> {
    template<typename Base>
    struct sentinel_impl {
        using OS = sentinel_t<Base>;
        OS outer_end_{};
        constexpr sentinel_impl() = default;
        constexpr explicit sentinel_impl(OS outer_end)
            : outer_end_(outer_end) {}
    };

    template<typename Base, typename Delimiter>
    struct iterator_impl {
        using O = iterator_t<Base>;
        using OS = sentinel_t<Base>;
        using OuterReference = range_reference_t<Base>;
        using InnerRange = remove_reference_t<OuterReference>;
        using I = iterator_t<InnerRange>;
        using S = sentinel_t<InnerRange>;
        using InnerValue = remove_cvref_t<range_reference_t<InnerRange>>;
        using value_type = common_type_t<InnerValue, D>;
        using difference_type = range_difference_t<InnerRange>;
        using reference = value_type;
        using pointer = void;
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;

        O outer_{};
        OS outer_end_{};
        I inner_{};
        S inner_end_{};
        Delimiter* delimiter_ = nullptr;
        bool emit_delimiter_ = false;
        bool done_ = true;

        constexpr iterator_impl() = default;

        constexpr iterator_impl(O outer, OS outer_end, Delimiter* delimiter)
            : outer_(outer), outer_end_(outer_end), inner_(), inner_end_(),
              delimiter_(delimiter), emit_delimiter_(false),
              done_(outer == outer_end) {
            if (!done_) {
                load_inner();
                if (inner_ == inner_end_)
                    advance_empty();
            }
        }

        constexpr void load_inner() {
            auto&& range = *outer_;
            inner_ = ranges::begin(range);
            inner_end_ = ranges::end(range);
        }

        constexpr void advance_empty() {
            ++outer_;
            if (outer_ == outer_end_) {
                done_ = true;
                return;
            }
            emit_delimiter_ = true;
            load_inner();
        }

        constexpr value_type operator*() const {
            if (emit_delimiter_)
                return value_type(*delimiter_);
            return value_type(*inner_);
        }

        constexpr iterator_impl& operator++() {
            if (done_) return *this;
            if (emit_delimiter_) {
                emit_delimiter_ = false;
                if (inner_ == inner_end_)
                    advance_empty();
                return *this;
            }
            ++inner_;
            if (inner_ == inner_end_)
                advance_empty();
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& left,
                                         const iterator_impl& right) {
            if (left.done_ || right.done_)
                return left.done_ == right.done_;
            return left.outer_ == right.outer_ &&
                   left.inner_ == right.inner_ &&
                   left.emit_delimiter_ == right.emit_delimiter_;
        }

        friend constexpr bool operator!=(const iterator_impl& left,
                                         const iterator_impl& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return value.done_ || value.outer_ == bound.outer_end_;
        }

        friend constexpr bool operator==(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return !(value == bound);
        }
    };

    using iterator = iterator_impl<V, D>;
    using sentinel = sentinel_impl<V>;

    V base_;
    D delimiter_;

public:
    join_with_view() = default;
    constexpr join_with_view(V base, D delimiter)
        : base_(std::move(base)), delimiter_(std::move(delimiter)) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator(ranges::begin(base_), ranges::end(base_), &delimiter_);
    }

    constexpr auto end() {
        return sentinel(ranges::end(base_));
    }

    constexpr auto begin() const
        requires join_with_view_base<const V, D> {
        return iterator_impl<const V, const D>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), &delimiter_);
    }

    constexpr auto end() const
        requires join_with_view_base<const V, D> {
        return sentinel_impl<const V>(ranges::end(std::as_const(base_)));
    }
};

template<typename R, typename D>
join_with_view(R&&, D&&)
    -> join_with_view<owning_view<R>, decay_t<D>>;

template<typename V, typename D>
inline constexpr bool enable_borrowed_range<join_with_view<V, D>> =
    borrowed_range<V>;
#endif



#if __cplusplus > 202002L
/* ═══════════════════════════════════════════════════════════════
 * concat_view - 二つの範囲を順に遅延連結
 * ═══════════════════════════════════════════════════════════════*/

template<typename V1, typename V2>
    requires range<V1> && range<V2> &&
             common_reference_with<range_reference_t<V1>,
                                   range_reference_t<V2>>
class concat_view : public view_interface<concat_view<V1, V2>> {
    using reference = common_reference_t<
        range_reference_t<V1>, range_reference_t<V2>>;

    template<bool Const>
    struct sentinel {
        using first_base = conditional_t<Const, const V1, V1>;
        using second_base = conditional_t<Const, const V2, V2>;
        using first_sentinel = sentinel_t<first_base>;
        using second_sentinel = sentinel_t<second_base>;

        first_sentinel first_end_;
        second_sentinel second_end_;

        constexpr sentinel(first_sentinel first_end,
                           second_sentinel second_end)
            : first_end_(std::move(first_end)),
              second_end_(std::move(second_end)) {}
    };

    template<bool Const>
    struct iterator {
        using first_base = conditional_t<Const, const V1, V1>;
        using second_base = conditional_t<Const, const V2, V2>;
        using first_iterator = iterator_t<first_base>;
        using first_sentinel = sentinel_t<first_base>;
        using second_iterator = iterator_t<second_base>;
        using second_sentinel = sentinel_t<second_base>;
        using element_reference = common_reference_t<
            range_reference_t<first_base>, range_reference_t<second_base>>;

        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = common_type_t<range_value_t<first_base>,
                                         range_value_t<second_base>>;
        using difference_type = ptrdiff_t;
        using pointer = void;
        using reference = element_reference;

        first_iterator first_{};
        first_sentinel first_end_{};
        second_iterator second_{};
        second_sentinel second_end_{};
        bool first_active_ = false;

        constexpr iterator() = default;

        constexpr iterator(first_iterator first, first_sentinel first_end,
                           second_iterator second,
                           second_sentinel second_end, bool first_active)
            : first_(std::move(first)), first_end_(std::move(first_end)),
              second_(std::move(second)), second_end_(std::move(second_end)),
              first_active_(first_active) {}

        constexpr reference operator*() const {
            if (first_active_) {
                return static_cast<reference>(*first_);
            }
            return static_cast<reference>(*second_);
        }

        constexpr iterator& operator++() {
            if (first_active_) {
                ++first_;
                if (first_ == first_end_) first_active_ = false;
            } else {
                ++second_;
            }
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            if (left.first_active_ != right.first_active_) return false;
            return left.first_active_ ? left.first_ == right.first_
                                      : left.second_ == right.second_;
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator& value,
                                         const sentinel<Const>& bound) {
            return value.first_active_ ? value.first_ == bound.first_end_
                                       : value.second_ == bound.second_end_;
        }

        friend constexpr bool operator==(const sentinel<Const>& bound,
                                         const iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel<Const>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel<Const>& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    V1 first_;
    V2 second_;

public:
    concat_view() = default;

    constexpr concat_view(V1 first, V2 second)
        : first_(std::move(first)), second_(std::move(second)) {}

    constexpr V1 base1() const& { return first_; }
    constexpr V1 base1() && { return std::move(first_); }
    constexpr V2 base2() const& { return second_; }
    constexpr V2 base2() && { return std::move(second_); }

    constexpr auto begin() {
        auto first = ranges::begin(first_);
        auto first_end = ranges::end(first_);
        auto second = ranges::begin(second_);
        auto second_end = ranges::end(second_);
        const bool first_active = !(first == first_end);
        return iterator<false>(std::move(first), std::move(first_end),
                               std::move(second), std::move(second_end),
                               first_active);
    }

    constexpr auto end() {
        return sentinel<false>(ranges::end(first_), ranges::end(second_));
    }

    constexpr auto begin() const
        requires range<const V1> && range<const V2> &&
                 common_reference_with<range_reference_t<const V1>,
                                        range_reference_t<const V2>> {
        auto first = ranges::begin(std::as_const(first_));
        auto first_end = ranges::end(std::as_const(first_));
        auto second = ranges::begin(std::as_const(second_));
        auto second_end = ranges::end(std::as_const(second_));
        const bool first_active = !(first == first_end);
        return iterator<true>(std::move(first), std::move(first_end),
                              std::move(second), std::move(second_end),
                              first_active);
    }

    constexpr auto end() const
        requires range<const V1> && range<const V2> &&
                 common_reference_with<range_reference_t<const V1>,
                                        range_reference_t<const V2>> {
        return sentinel<true>(ranges::end(std::as_const(first_)),
                              ranges::end(std::as_const(second_)));
    }

    constexpr size_t size() const
        requires sized_range<V1> && sized_range<V2> {
        const size_t first_size = static_cast<size_t>(ranges::size(first_));
        const size_t second_size = static_cast<size_t>(ranges::size(second_));
        const size_t maximum = static_cast<size_t>(-1);
        return second_size > maximum - first_size
            ? maximum : first_size + second_size;
    }
};

template<typename R1, typename R2>
concat_view(R1&&, R2&&)
    -> concat_view<owning_view<R1>, owning_view<R2>>;
/* A concatenation is borrowed only when every stored view is borrowed. */
template<typename V1, typename V2>
inline constexpr bool enable_borrowed_range<concat_view<V1, V2>> =
    borrowed_range<V1> && borrowed_range<V2>;
#endif

/* ???????????????????????????????????????????????????????????????
 * split_view - delimiter value ????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V, typename T>
    requires forward_range<V>
class split_view : public view_interface<split_view<V, T>> {
    V base_;
    T delimiter_;
    template<typename Base>
    struct sentinel_impl {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};
        constexpr sentinel_impl() = default;
        constexpr explicit sentinel_impl(base_sentinel end) : end_(end) {}
    };

    template<typename Base, typename Delimiter>
    struct iterator_impl {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        using iterator_category = forward_iterator_tag;
        using iterator_concept = forward_iterator_tag;
        using value_type = subrange<base_iterator, base_sentinel>;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using reference = value_type;
        base_iterator current_{};
        base_sentinel end_{};
        Delimiter* delimiter_ = nullptr;
        bool trailing_empty_ = false;
        bool done_ = false;

        constexpr iterator_impl() = default;
        constexpr iterator_impl(base_iterator current, base_sentinel end,
                                Delimiter* delimiter)
            : current_(current), end_(end), delimiter_(delimiter),
              trailing_empty_(false), done_(false) {}

        constexpr base_iterator boundary() const {
            auto last = current_;
            while (last != end_ && !(*last == *delimiter_)) ++last;
            return last;
        }

        constexpr value_type operator*() const {
            return value_type(current_, boundary());
        }

        constexpr iterator_impl& operator++() {
            if (done_) return *this;
            if (trailing_empty_) {
                trailing_empty_ = false;
                done_ = true;
                return *this;
            }
            auto last = boundary();
            if (last == end_) {
                current_ = last;
                done_ = true;
                return *this;
            }
            ++last;
            current_ = last;
            if (current_ == end_) trailing_empty_ = true;
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& a,
                                         const iterator_impl& b) {
            return a.done_ == b.done_ &&
                   (a.done_ || (a.current_ == b.current_ &&
                                a.trailing_empty_ == b.trailing_empty_));
        }
        friend constexpr bool operator!=(const iterator_impl& a,
                                         const iterator_impl& b) { return !(a == b); }
        friend constexpr bool operator==(const iterator_impl& i,
                                         const sentinel_impl<Base>&) { return i.done_; }
        friend constexpr bool operator!=(const iterator_impl& i,
                                         const sentinel_impl<Base>& s) { return !(i == s); }
        friend constexpr bool operator==(const sentinel_impl<Base>& s,
                                         const iterator_impl& i) { return i == s; }
        friend constexpr bool operator!=(const sentinel_impl<Base>& s,
                                         const iterator_impl& i) { return !(i == s); }
    };

    using iterator = iterator_impl<V, T>;
    using sentinel = sentinel_impl<V>;

public:
    split_view() = default;
    constexpr split_view(V base, T delimiter)
        : base_(std::move(base)), delimiter_(std::move(delimiter)) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }
    constexpr auto begin() {
        return iterator(ranges::begin(base_), ranges::end(base_), &delimiter_);
    }
    constexpr auto end() { return sentinel(ranges::end(base_)); }

    constexpr auto begin() const
        requires forward_range<const V> {
        return iterator_impl<const V, const T>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), &delimiter_);
    }
    constexpr auto end() const
        requires forward_range<const V> {
        return sentinel_impl<const V>(ranges::end(std::as_const(base_)));
    }
};

template<typename V, typename P>
    requires forward_range<V> && forward_range<P>
class split_pattern_view : public view_interface<split_pattern_view<V, P>> {
    V base_;
    P pattern_;

    template<typename Base>
    struct sentinel_impl {};
    template<typename Base, typename Pattern>
    struct iterator_impl {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        using iterator_category = forward_iterator_tag;
        using iterator_concept = forward_iterator_tag;
        using value_type = subrange<base_iterator, base_sentinel>;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using reference = value_type;
        base_iterator current_{};
        base_sentinel end_{};
        Pattern* pattern_ = nullptr;
        bool trailing_empty_ = false;
        bool done_ = false;

        constexpr bool matches(base_iterator candidate) const {
            auto pattern = ranges::begin(*pattern_);
            auto pattern_end = ranges::end(*pattern_);
            if (pattern == pattern_end) return false;
            while (pattern != pattern_end) {
                if (candidate == end_ || !(*candidate == *pattern))
                    return false;
                ++candidate;
                ++pattern;
            }
            return true;
        }

        constexpr base_iterator boundary() const {
            auto candidate = current_;
            while (candidate != end_ && !matches(candidate)) ++candidate;
            return candidate;
        }

        constexpr iterator_impl(base_iterator current, base_sentinel end,
                                Pattern* pattern)
            : current_(current), end_(end), pattern_(pattern) {}

        constexpr value_type operator*() const {
            return value_type(current_, boundary());
        }

        constexpr iterator_impl& operator++() {
            if (done_) return *this;
            if (trailing_empty_) {
                trailing_empty_ = false;
                done_ = true;
                return *this;
            }
            auto last = boundary();
            if (last == end_) {
                current_ = last;
                done_ = true;
                return *this;
            }
            auto pattern = ranges::begin(*pattern_);
            auto pattern_end = ranges::end(*pattern_);
            while (pattern != pattern_end) {
                ++last;
                ++pattern;
            }
            current_ = last;
            if (current_ == end_) trailing_empty_ = true;
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& left,
                                         const iterator_impl& right) {
            return left.done_ == right.done_ &&
                   (left.done_ || (left.current_ == right.current_ &&
                                   left.trailing_empty_ == right.trailing_empty_));
        }
        friend constexpr bool operator!=(const iterator_impl& left,
                                         const iterator_impl& right) { return !(left == right); }
        friend constexpr bool operator==(const iterator_impl& value,
                                         const sentinel_impl<Base>&) { return value.done_; }
        friend constexpr bool operator!=(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) { return !(value == bound); }
        friend constexpr bool operator==(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) { return value == bound; }
        friend constexpr bool operator!=(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) { return !(value == bound); }
    };

    using iterator = iterator_impl<V, P>;
    using sentinel = sentinel_impl<V>;

public:
    constexpr const V& base() const& noexcept { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr split_pattern_view(V base, P pattern)
        : base_(std::move(base)), pattern_(std::move(pattern)) {}
    constexpr auto begin() { return iterator(ranges::begin(base_), ranges::end(base_), &pattern_); }
    constexpr auto end() { return sentinel{}; }

    constexpr auto begin() const
        requires forward_range<const V> && forward_range<const P> {
        return iterator_impl<const V, const P>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), &pattern_);
    }
    constexpr auto end() const
        requires forward_range<const V> && forward_range<const P> {
        return sentinel_impl<const V>{};
    }
};

/* Input-only lazy_split value delimiter.  The regular split_view requires a
 * forward base so its outer cursor can be revisited; lazy_split is also
 * permitted to consume a single-pass input range. */
template<typename V, typename T>
    requires input_range<V>
class lazy_split_input_view
    : public view_interface<lazy_split_input_view<V, T>> {
    V base_;
    T delimiter_;
    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    struct sentinel {
        base_sentinel end_{};
        constexpr explicit sentinel(base_sentinel end) : end_(end) {}
    };

    struct iterator {
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = subrange<base_iterator, base_iterator>;
        using difference_type = range_difference_t<V>;
        using pointer = void;
        using reference = value_type;
        base_iterator current_{};
        base_sentinel end_{};
        T* delimiter_ = nullptr;
        bool trailing_empty_ = false;
        bool done_ = false;

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end, T* delimiter)
            : current_(current), end_(end), delimiter_(delimiter) {}
        constexpr base_iterator boundary() const {
            auto last = current_;
            while (last != end_ && !(*last == *delimiter_)) ++last;
            return last;
        }
        constexpr value_type operator*() const {
            return value_type(current_, boundary());
        }
        constexpr iterator& operator++() {
            if (done_) return *this;
            if (trailing_empty_) {
                trailing_empty_ = false;
                done_ = true;
                return *this;
            }
            auto last = boundary();
            if (last == end_) {
                current_ = last;
                done_ = true;
                return *this;
            }
            ++last;
            current_ = last;
            if (current_ == end_) trailing_empty_ = true;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.done_ == right.done_ &&
                   (left.done_ || (left.current_ == right.current_ &&
                                   left.trailing_empty_ == right.trailing_empty_));
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
        friend constexpr bool operator==(const iterator& value,
                                         const sentinel& bound) {
            return value.done_ || value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

public:
    constexpr lazy_split_input_view(V base, T delimiter)
        : base_(std::move(base)), delimiter_(std::move(delimiter)) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }
    constexpr auto begin() {
        return iterator(ranges::begin(base_), ranges::end(base_), &delimiter_);
    }
    constexpr auto end() { return sentinel(ranges::end(base_)); }
};

template<typename V, typename T>
inline constexpr bool enable_borrowed_range<lazy_split_input_view<V, T>> =
    borrowed_range<V>;

template<typename V, typename P>
    requires input_range<V> && forward_range<P>
class lazy_split_input_pattern_view
    : public view_interface<lazy_split_input_pattern_view<V, P>> {
    V base_;
    P pattern_;
    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    struct sentinel {
        base_sentinel end_{};
        constexpr explicit sentinel(base_sentinel end) : end_(end) {}
    };

    struct iterator {
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = subrange<base_iterator, base_iterator>;
        using difference_type = range_difference_t<V>;
        using pointer = void;
        using reference = value_type;
        base_iterator current_{};
        base_sentinel end_{};
        P* pattern_ = nullptr;
        bool trailing_empty_ = false;
        bool done_ = false;

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end, P* pattern)
            : current_(current), end_(end), pattern_(pattern) {}

        constexpr bool matches(base_iterator candidate) const {
            auto pattern = ranges::begin(*pattern_);
            auto pattern_end = ranges::end(*pattern_);
            while (pattern != pattern_end) {
                if (candidate == end_ || !(*candidate == *pattern))
                    return false;
                ++candidate;
                ++pattern;
            }
            return true;
        }

        constexpr base_iterator boundary() const {
            auto last = current_;
            while (last != end_ && !matches(last)) ++last;
            return last;
        }

        constexpr value_type operator*() const {
            return value_type(current_, boundary());
        }

        constexpr iterator& operator++() {
            if (done_) return *this;
            if (trailing_empty_) {
                trailing_empty_ = false;
                done_ = true;
                return *this;
            }
            auto last = boundary();
            if (last == end_) {
                current_ = last;
                done_ = true;
                return *this;
            }
            auto pattern = ranges::begin(*pattern_);
            auto pattern_end = ranges::end(*pattern_);
            while (pattern != pattern_end) {
                ++last;
                ++pattern;
            }
            current_ = last;
            if (current_ == end_) trailing_empty_ = true;
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.done_ == right.done_ &&
                   (left.done_ || (left.current_ == right.current_ &&
                                   left.trailing_empty_ == right.trailing_empty_));
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
        friend constexpr bool operator==(const iterator& value,
                                         const sentinel& bound) {
            return value.done_ || value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

public:
    constexpr lazy_split_input_pattern_view(V base, P pattern)
        : base_(std::move(base)), pattern_(std::move(pattern)) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }
    constexpr auto begin() {
        return iterator(ranges::begin(base_), ranges::end(base_), &pattern_);
    }
    constexpr auto end() { return sentinel(ranges::end(base_)); }
};

template<typename V, typename P>
inline constexpr bool enable_borrowed_range<lazy_split_input_pattern_view<V, P>> =
    borrowed_range<V>;

template<typename R, typename T>
split_view(R&&, T) -> split_view<owning_view<R>, decay_t<T>>;

template<typename V, typename T>
inline constexpr bool enable_borrowed_range<split_view<V, T>> =
    borrowed_range<V>;

template<typename V, typename P>
inline constexpr bool enable_borrowed_range<split_pattern_view<V, P>> =
    borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * transform_view - ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V, typename F>
    requires input_range<V> && copy_constructible<F> &&
             regular_invocable<F&, range_reference_t<V>>
class transform_view : public view_interface<transform_view<V, F>> {
    V base_;
    F func_;

    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using function_type = conditional_t<is_const_v<Base>, const F, F>;
        using iterator_category = typename iterator_traits<base_iterator>::iterator_category;
        using iterator_concept = iterator_category;
        using value_type = remove_cvref_t<decltype(std::invoke(
            declval<function_type&>(), *declval<base_iterator&>()))>;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using reference = decltype(std::invoke(
            declval<function_type&>(), *declval<base_iterator&>()));

        base_iterator current_{};
        function_type* function_ = nullptr;

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, function_type* function)
            : current_(std::move(current)), function_(function) {}

        constexpr decltype(auto) operator*() const {
            return std::invoke(*function_, *current_);
        }
        constexpr iterator& operator++() { ++current_; return *this; }
        constexpr iterator operator++(int) { auto copy = *this; ++*this; return copy; }

        constexpr iterator& operator--()
            requires bidirectional_iterator<base_iterator> {
            --current_;
            return *this;
        }
        constexpr iterator operator--(int)
            requires bidirectional_iterator<base_iterator> {
            auto copy = *this;
            --*this;
            return copy;
        }

        constexpr iterator& operator+=(difference_type offset)
            requires random_access_iterator<base_iterator> {
            current_ += offset;
            return *this;
        }
        constexpr iterator& operator-=(difference_type offset)
            requires random_access_iterator<base_iterator> {
            current_ -= offset;
            return *this;
        }
        constexpr decltype(auto) operator[](difference_type offset) const
            requires random_access_iterator<base_iterator> {
            return std::invoke(*function_, current_[offset]);
        }

        template<typename Other>
        friend constexpr bool operator==(const iterator& left,
                                         const iterator<Other>& right) {
            return left.current_ == right.current_;
        }
        template<typename Other>
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator<Other>& right) {
            return !(left == right);
        }
        friend constexpr iterator operator+(iterator value, difference_type offset)
            requires random_access_iterator<base_iterator> {
            value += offset;
            return value;
        }
        friend constexpr iterator operator+(difference_type offset, iterator value)
            requires random_access_iterator<base_iterator> {
            value += offset;
            return value;
        }
        friend constexpr iterator operator-(iterator value, difference_type offset)
            requires random_access_iterator<base_iterator> {
            value -= offset;
            return value;
        }
        friend constexpr difference_type operator-(const iterator& left,
                                                   const iterator& right)
            requires random_access_iterator<base_iterator> {
            return left.current_ - right.current_;
        }
    };

    template<typename Base>
    struct sentinel {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};

        constexpr sentinel() = default;
        constexpr explicit sentinel(base_sentinel end) : end_(std::move(end)) {}

        friend constexpr bool operator==(const iterator<Base>& value,
                                         const sentinel& bound) {
            return value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator<Base>& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator<Base>& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator<Base>& value) {
            return !(value == bound);
        }
    };

public:
    transform_view() = default;
    constexpr transform_view(V base, F function)
        : base_(std::move(base)), func_(std::move(function)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), &func_);
    }
    constexpr auto end() {
        if constexpr (same_as<iterator_t<V>, sentinel_t<V>>) {
            return iterator<V>(ranges::end(base_), &func_);
        } else {
            return sentinel<V>(ranges::end(base_));
        }
    }

    constexpr auto begin() const
        requires range<const V> &&
                 regular_invocable<const F&, range_reference_t<const V>> {
        return iterator<const V>(ranges::begin(std::as_const(base_)), &func_);
    }
    constexpr auto end() const
        requires range<const V> &&
                 regular_invocable<const F&, range_reference_t<const V>> {
        if constexpr (same_as<iterator_t<const V>, sentinel_t<const V>>) {
            return iterator<const V>(
                ranges::end(std::as_const(base_)), &func_);
        } else {
            return sentinel<const V>(ranges::end(std::as_const(base_)));
        }
    }

    constexpr auto size() requires sized_range<V> {
        return ranges::size(base_);
    }
    constexpr auto size() const requires sized_range<const V> {
        return ranges::size(std::as_const(base_));
    }
};

template<typename V, typename F>
transform_view(V, F) -> transform_view<V, F>;

template<typename V, typename F>
inline constexpr bool enable_borrowed_range<transform_view<V, F>> =
    borrowed_range<V>;

/* ═══════════════════════════════════════════════════════════════
 * filter_view - フィルタービュー
 * ═══════════════════════════════════════════════════════════════*/
template<typename V, typename Pred>
    requires input_range<V> && copy_constructible<Pred> &&
             predicate<Pred&, range_reference_t<V>>
class filter_view : public view_interface<filter_view<V, Pred>> {
    V base_;
    Pred pred_;

    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        using predicate_type = conditional_t<is_const_v<Base>, const Pred, Pred>;
        using iterator_category = typename iterator_traits<base_iterator>::iterator_category;
        using iterator_concept = iterator_category;
        using value_type = range_value_t<Base>;
        using difference_type = range_difference_t<Base>;
        using pointer = typename iterator_traits<base_iterator>::pointer;
        using reference = range_reference_t<Base>;

        base_iterator current_{};
        base_sentinel end_{};
        predicate_type* predicate_ = nullptr;

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end,
                           predicate_type* predicate)
            : current_(std::move(current)), end_(std::move(end)),
              predicate_(predicate) {
            satisfy();
        }

        constexpr void satisfy() {
            while (current_ != end_ &&
                   !static_cast<bool>((*predicate_)(*current_))) {
                ++current_;
            }
        }

        constexpr reference operator*() const { return *current_; }
        constexpr pointer operator->() const
            requires requires(base_iterator value) { value.operator->(); } {
            return current_.operator->();
        }
        constexpr iterator& operator++() {
            ++current_;
            satisfy();
            return *this;
        }
        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.current_ == right.current_;
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
    };

    template<typename Base>
    struct sentinel {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};

        constexpr sentinel() = default;
        constexpr explicit sentinel(base_sentinel end) : end_(std::move(end)) {}

        friend constexpr bool operator==(const iterator<Base>& value,
                                         const sentinel& bound) {
            return value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator<Base>& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator<Base>& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator<Base>& value) {
            return !(value == bound);
        }
    };

public:
    filter_view() = default;
    constexpr filter_view(V base, Pred predicate)
        : base_(std::move(base)), pred_(std::move(predicate)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_), &pred_);
    }
    constexpr auto end() {
        if constexpr (same_as<iterator_t<V>, sentinel_t<V>>) {
            return iterator<V>(ranges::end(base_), ranges::end(base_), &pred_);
        } else {
            return sentinel<V>(ranges::end(base_));
        }
    }

    constexpr auto begin() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        return iterator<const V>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), &pred_);
    }
    constexpr auto end() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        if constexpr (same_as<iterator_t<const V>, sentinel_t<const V>>) {
            return iterator<const V>(
                ranges::end(std::as_const(base_)),
                ranges::end(std::as_const(base_)), &pred_);
        } else {
            return sentinel<const V>(ranges::end(std::as_const(base_)));
        }
    }
};

template<typename V, typename Pred>
filter_view(V, Pred) -> filter_view<V, Pred>;

template<typename V, typename Pred>
inline constexpr bool enable_borrowed_range<filter_view<V, Pred>> =
    borrowed_range<V>;

template<typename V, typename Pred>
    requires input_range<V> && copy_constructible<Pred> &&
             predicate<Pred&, range_reference_t<V>>
class take_while_view : public view_interface<take_while_view<V, Pred>> {
    V base_;
    Pred pred_;

    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        using predicate_type = conditional_t<is_const_v<Base>, const Pred, Pred>;
        using iterator_category = input_iterator_tag;
        using iterator_concept = iterator_category;
        using value_type = iter_value_t<base_iterator>;
        using difference_type = range_difference_t<Base>;
        using pointer = typename iterator_traits<base_iterator>::pointer;
        using reference = range_reference_t<Base>;

        base_iterator current_{};
        base_sentinel end_{};
        predicate_type* predicate_ = nullptr;
        bool done_ = false;

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end,
                           predicate_type* predicate)
            : current_(std::move(current)), end_(std::move(end)),
              predicate_(predicate) {
            satisfy();
        }

        constexpr void satisfy() {
            done_ = current_ == end_;
            if (!done_)
                done_ = !static_cast<bool>((*predicate_)(*current_));
        }

        constexpr reference operator*() const { return *current_; }
        constexpr pointer operator->() const
            requires requires(const base_iterator& value) {
                value.operator->();
            } {
            return current_.operator->();
        }
        constexpr iterator& operator++() {
            if (!done_) {
                ++current_;
                satisfy();
            }
            return *this;
        }
        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return (left.done_ && right.done_) ||
                   left.current_ == right.current_;
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
    };

    template<typename Base>
    struct sentinel {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};

        constexpr sentinel() = default;
        constexpr explicit sentinel(base_sentinel end) : end_(std::move(end)) {}

        friend constexpr bool operator==(const iterator<Base>& value,
                                         const sentinel& bound) {
            return value.done_ || value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator<Base>& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator<Base>& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator<Base>& value) {
            return !(value == bound);
        }
    };

public:
    take_while_view() = default;
    constexpr take_while_view(V base, Pred predicate)
        : base_(std::move(base)), pred_(std::move(predicate)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_), &pred_);
    }
    constexpr auto end() {
        if constexpr (same_as<iterator_t<V>, sentinel_t<V>>) {
            return iterator<V>(ranges::end(base_), ranges::end(base_), &pred_);
        } else {
            return sentinel<V>(ranges::end(base_));
        }
    }

    constexpr auto begin() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        return iterator<const V>(ranges::begin(std::as_const(base_)),
                                 ranges::end(std::as_const(base_)), &pred_);
    }
    constexpr auto end() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        if constexpr (same_as<iterator_t<const V>, sentinel_t<const V>>) {
            return iterator<const V>(
                ranges::end(std::as_const(base_)),
                ranges::end(std::as_const(base_)), &pred_);
        } else {
            return sentinel<const V>(ranges::end(std::as_const(base_)));
        }
    }
};

template<typename V, typename Pred>
take_while_view(V, Pred) -> take_while_view<V, Pred>;

template<typename V, typename Pred>
inline constexpr bool enable_borrowed_range<take_while_view<V, Pred>> =
    borrowed_range<V>;

template<typename V, typename Pred>
    requires input_range<V> && copy_constructible<Pred> &&
             predicate<Pred&, range_reference_t<V>>
class drop_while_view : public view_interface<drop_while_view<V, Pred>> {
    V base_;
    Pred pred_;

public:
    drop_while_view() = default;
    constexpr drop_while_view(V base, Pred predicate)
        : base_(std::move(base)), pred_(std::move(predicate)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        auto it = ranges::begin(base_);
        auto e = ranges::end(base_);
        while (it != e && static_cast<bool>(pred_(*it))) ++it;
        return it;
    }

    constexpr auto end() { return ranges::end(base_); }

    constexpr auto begin() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        auto it = ranges::begin(std::as_const(base_));
        auto e = ranges::end(std::as_const(base_));
        while (it != e && static_cast<bool>(pred_(*it))) ++it;
        return it;
    }

    constexpr auto end() const
        requires range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>> {
        return ranges::end(std::as_const(base_));
    }
};

template<typename V, typename Pred>
drop_while_view(V, Pred) -> drop_while_view<V, Pred>;

template<typename V, typename Pred>
inline constexpr bool enable_borrowed_range<drop_while_view<V, Pred>> =
    borrowed_range<V>;

template<typename V>
    requires input_range<V>
class common_view : public view_interface<common_view<V>> {
    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        using iterator_category = input_iterator_tag;
        using iterator_concept = iterator_category;
        using value_type = iter_value_t<base_iterator>;
        using difference_type = iter_difference_t<base_iterator>;
        using pointer = typename iterator_traits<base_iterator>::pointer;
        using reference = iter_reference_t<base_iterator>;
        base_iterator current_{};
        base_sentinel end_{};
        bool done_ = true;
        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end, bool done)
            : current_(std::move(current)), end_(std::move(end)), done_(done) {
            if (!done_) done_ = current_ == end_;
        }
        constexpr reference operator*() const { return *current_; }
        constexpr pointer operator->() const
            requires requires(const base_iterator& value) { value.operator->(); } {
            return current_.operator->();
        }
        constexpr iterator& operator++() {
            if (!done_) { ++current_; done_ = current_ == end_; }
            return *this;
        }
        constexpr iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        friend constexpr bool operator==(const iterator& left, const iterator& right) {
            return (left.done_ && right.done_) ||
                   (!left.done_ && !right.done_ && left.current_ == right.current_);
        }
        friend constexpr bool operator!=(const iterator& left, const iterator& right) {
            return !(left == right);
        }
    };
    V base_;
public:
    common_view() = default;
    constexpr explicit common_view(V base) : base_(std::move(base)) {}
    constexpr const V& base() const& noexcept { return base_; }
    constexpr V&& base() && noexcept { return std::move(base_); }
    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_), false);
    }
    constexpr auto end() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_), true);
    }
    constexpr auto begin() const requires input_range<const V> {
        return iterator<const V>(ranges::begin(std::as_const(base_)),
                                 ranges::end(std::as_const(base_)), false);
    }
    constexpr auto end() const requires input_range<const V> {
        return iterator<const V>(ranges::begin(std::as_const(base_)),
                                 ranges::end(std::as_const(base_)), true);
    }
    constexpr auto size() requires sized_range<V> { return ranges::size(base_); }
    constexpr auto size() const requires sized_range<const V> {
        return ranges::size(std::as_const(base_));
    }
};

template<typename R>
common_view(R&&) -> common_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<common_view<V>> = borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * take_view - ??N????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires input_range<V>
class take_view : public view_interface<take_view<V>> {
    V base_;
    range_difference_t<V> count_;

    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using base_sentinel = sentinel_t<Base>;
        /* The count-aware cursor currently exposes only the input operations
         * it implements.  Do not inherit a stronger base category until the
         * random-access arithmetic also clamps the take boundary. */
        using iterator_category = input_iterator_tag;
        using iterator_concept = iterator_category;
        using value_type = iter_value_t<base_iterator>;
        using difference_type = range_difference_t<Base>;
        using pointer = typename iterator_traits<base_iterator>::pointer;
        using reference = range_reference_t<Base>;

        base_iterator current_{};
        base_sentinel end_{};
        difference_type remaining_{};

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, base_sentinel end,
                           difference_type remaining)
            : current_(std::move(current)), end_(std::move(end)),
              remaining_(remaining) {}

        constexpr reference operator*() const { return *current_; }
        constexpr pointer operator->() const
            requires requires(const base_iterator& value) { value.operator->(); } {
            return current_.operator->();
        }
        constexpr iterator& operator++() {
            if (remaining_ > 0 && current_ != end_) {
                ++current_;
                --remaining_;
            }
            return *this;
        }
        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return (left.remaining_ <= 0 && right.remaining_ <= 0) ||
                   left.current_ == right.current_;
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
    };

    template<typename Base>
    struct sentinel {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};

        constexpr sentinel() = default;
        constexpr explicit sentinel(base_sentinel end) : end_(std::move(end)) {}

        friend constexpr bool operator==(const iterator<Base>& value,
                                         const sentinel& bound) {
            return value.remaining_ <= 0 || value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator<Base>& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator<Base>& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator<Base>& value) {
            return !(value == bound);
        }
    };

public:
    take_view() = default;
    constexpr take_view(V base, range_difference_t<V> count)
        : base_(std::move(base)), count_(count) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_), count_);
    }

    constexpr auto end() {
        if constexpr (same_as<iterator_t<V>, sentinel_t<V>>) {
            return iterator<V>(ranges::end(base_), ranges::end(base_), 0);
        } else {
            return sentinel<V>(ranges::end(base_));
        }
    }

    constexpr auto begin() const
        requires input_range<const V> {
        return iterator<const V>(ranges::begin(std::as_const(base_)),
                                 ranges::end(std::as_const(base_)), count_);
    }

    constexpr auto end() const
        requires input_range<const V> {
        if constexpr (same_as<iterator_t<const V>, sentinel_t<const V>>) {
            return iterator<const V>(
                ranges::end(std::as_const(base_)),
                ranges::end(std::as_const(base_)), 0);
        } else {
            return sentinel<const V>(ranges::end(std::as_const(base_)));
        }
    }

    constexpr auto size() requires sized_range<V> {
        using size_type = decltype(ranges::size(base_));
        auto s = ranges::size(base_);
        /* views::take treats a non-positive count as an empty prefix.  Do
         * not cast a negative signed count to the unsigned range size type;
         * that would publish a near-maximum size for an empty view. */
        if (count_ <= 0) return static_cast<size_type>(0);
        const size_type requested = static_cast<size_type>(count_);
        return s < requested ? s : requested;
    }

    constexpr auto size() const requires sized_range<const V> {
        using size_type = decltype(ranges::size(std::as_const(base_)));
        auto s = ranges::size(std::as_const(base_));
        if (count_ <= 0) return static_cast<size_type>(0);
        const size_type requested = static_cast<size_type>(count_);
        return s < requested ? s : requested;
    }
};

template<typename R>
take_view(R&&, range_difference_t<R>) -> take_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<take_view<V>> = borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * drop_view - ??N??????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires input_range<V>
class drop_view : public view_interface<drop_view<V>> {
    V base_;
    range_difference_t<V> count_;

public:
    drop_view() = default;
    constexpr drop_view(V base, range_difference_t<V> count)
        : base_(std::move(base)), count_(count) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        auto it = ranges::begin(base_);
        auto e = ranges::end(base_);
        for (range_difference_t<V> i = 0; i < count_ && it != e; ++i, ++it) {}
        return it;
    }

    constexpr auto end() { return ranges::end(base_); }

    constexpr auto begin() const
        requires input_range<const V> {
        auto it = ranges::begin(std::as_const(base_));
        auto e = ranges::end(std::as_const(base_));
        for (range_difference_t<const V> i = 0;
             i < count_ && it != e; ++i, ++it) {}
        return it;
    }

    constexpr auto end() const
        requires input_range<const V> {
        return ranges::end(std::as_const(base_));
    }

    constexpr auto size() requires sized_range<V> {
        using size_type = decltype(ranges::size(base_));
        auto s = ranges::size(base_);
        if (count_ <= 0) return s;
        const size_type requested = static_cast<size_type>(count_);
        return s < requested ? static_cast<size_type>(0) : s - requested;
    }

    constexpr auto size() const requires sized_range<const V> {
        using size_type = decltype(ranges::size(std::as_const(base_)));
        auto s = ranges::size(std::as_const(base_));
        if (count_ <= 0) return s;
        const size_type requested = static_cast<size_type>(count_);
        return s < requested ? static_cast<size_type>(0) : s - requested;
    }
};

template<typename R>
drop_view(R&&, range_difference_t<R>) -> drop_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<drop_view<V>> = borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * reverse_view - ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires bidirectional_range<V> && common_range<V>
class reverse_view : public view_interface<reverse_view<V>> {
    V base_;

public:
    reverse_view() = default;
    constexpr explicit reverse_view(V base) : base_(std::move(base)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return make_reverse_iterator(ranges::end(base_));
    }

    constexpr auto end() {
        return make_reverse_iterator(ranges::begin(base_));
    }

    constexpr auto begin() const
        requires bidirectional_range<const V> && common_range<const V> {
        return make_reverse_iterator(ranges::end(std::as_const(base_)));
    }

    constexpr auto end() const
        requires bidirectional_range<const V> && common_range<const V> {
        return make_reverse_iterator(ranges::begin(std::as_const(base_)));
    }
};

template<typename R>
reverse_view(R&&) -> reverse_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<reverse_view<V>> = borrowed_range<V>;

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * as_const_view - const??????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename I>
class basic_const_iterator;

template<typename T>
struct is_basic_const_iterator : false_type {};

template<typename I>
struct is_basic_const_iterator<basic_const_iterator<I>> : true_type {};

template<typename I>
class basic_const_iterator {
    I current_;

public:
    using iterator_category = input_iterator_tag;
    using value_type = remove_cvref_t<decltype(*declval<I&>())>;
    using difference_type = iter_difference_t<I>;
    using pointer = void;
    using reference = decltype(std::as_const(*declval<I&>()));

    constexpr basic_const_iterator() = default;
    constexpr explicit basic_const_iterator(I current) : current_(current) {}

    constexpr decltype(auto) operator*() const {
        return std::as_const(*current_);
    }

    constexpr basic_const_iterator& operator++() {
        ++current_;
        return *this;
    }

    constexpr basic_const_iterator operator++(int) {
        auto copy = *this;
        ++*this;
        return copy;
    }

    constexpr auto operator->() const { return current_; }

    friend constexpr bool operator==(const basic_const_iterator& left,
                                     const basic_const_iterator& right) {
        return left.current_ == right.current_;
    }

    friend constexpr bool operator!=(const basic_const_iterator& left,
                                     const basic_const_iterator& right) {
        return !(left == right);
    }

    template<typename S>
        requires (!is_basic_const_iterator<remove_cvref_t<S>>::value) &&
                 requires(const I& it, const S& sentinel) {
            { it == sentinel } -> convertible_to<bool>;
        }
    friend constexpr bool operator==(const basic_const_iterator& it,
                                     const S& sentinel) {
        return it.current_ == sentinel;
    }

    template<typename S>
        requires (!is_basic_const_iterator<remove_cvref_t<S>>::value) &&
                 requires(const I& it, const S& sentinel) {
            { it == sentinel } -> convertible_to<bool>;
        }
    friend constexpr bool operator==(const S& sentinel,
                                     const basic_const_iterator& it) {
        return it == sentinel;
    }

    template<typename S>
        requires (!is_basic_const_iterator<remove_cvref_t<S>>::value) &&
                 requires(const I& it, const S& sentinel) {
            { it == sentinel } -> convertible_to<bool>;
        }
    friend constexpr bool operator!=(const basic_const_iterator& it,
                                     const S& sentinel) {
        return !(it == sentinel);
    }

    template<typename S>
        requires (!is_basic_const_iterator<remove_cvref_t<S>>::value) &&
                 requires(const I& it, const S& sentinel) {
            { it == sentinel } -> convertible_to<bool>;
        }
    friend constexpr bool operator!=(const S& sentinel,
                                     const basic_const_iterator& it) {
        return !(sentinel == it);
    }
};

template<typename V>
    requires view<V> && input_range<V>
class as_const_view : public view_interface<as_const_view<V>> {
    V base_;

public:
    as_const_view() = default;
    constexpr explicit as_const_view(V base) : base_(std::move(base)) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        using iterator = iterator_t<V>;
        return basic_const_iterator<iterator>(ranges::begin(base_));
    }

    constexpr auto end() {
        using iterator = iterator_t<V>;
        using sentinel = sentinel_t<V>;
        if constexpr (is_same_v<iterator, sentinel>) {
            return basic_const_iterator<iterator>(ranges::end(base_));
        } else {
            return ranges::end(base_);
        }
    }

    constexpr auto begin() const {
        using iterator = decltype(ranges::begin(std::as_const(base_)));
        return basic_const_iterator<iterator>(
            ranges::begin(std::as_const(base_)));
    }

    constexpr auto end() const {
        using iterator = decltype(ranges::begin(std::as_const(base_)));
        using sentinel = decltype(ranges::end(std::as_const(base_)));
        if constexpr (is_same_v<iterator, sentinel>) {
            return basic_const_iterator<iterator>(
                ranges::end(std::as_const(base_)));
        } else {
            return ranges::end(std::as_const(base_));
        }
    }

    constexpr auto size() requires sized_range<V> {
        return ranges::size(base_);
    }
};

template<typename R>
    requires viewable_range<R> && input_range<R>
as_const_view(R&&) -> as_const_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<as_const_view<V>> =
    borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * zip_view - ???????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V1, typename V2>
    requires input_range<V1> && input_range<V2>
class zip_view : public view_interface<zip_view<V1, V2>> {
    using I1 = iterator_t<V1>;
    using I2 = iterator_t<V2>;
    using S1 = sentinel_t<V1>;
    using S2 = sentinel_t<V2>;

    template<typename B1, typename B2>
    struct sentinel;

    template<typename B1, typename B2>
    struct iterator {
        using I1 = iterator_t<B1>;
        using I2 = iterator_t<B2>;
        using reference = pair<iter_reference_t<I1>, iter_reference_t<I2>>;
        using value_type = remove_cvref_t<reference>;
        using difference_type = common_type_t<iter_difference_t<I1>,
                                              iter_difference_t<I2>>;
        using pointer = void;
        using iterator_category = input_iterator_tag;
        I1 first_;
        I2 second_;

        constexpr iterator() = default;
        constexpr iterator(I1 first, I2 second)
            : first_(first), second_(second) {}

        constexpr auto operator*() const {
            return std::pair<decltype(*first_), decltype(*second_)>(
                *first_, *second_);
        }

        constexpr iterator& operator++() {
            ++first_;
            ++second_;
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.first_ == right.first_ &&
                   left.second_ == right.second_;
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        template<typename S,
                 enable_if_t<is_same_v<S, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const iterator& value,
                                         const S& bound) {
            return value.first_ == bound.first_ ||
                   value.second_ == bound.second_;
        }

        template<typename S,
                 enable_if_t<is_same_v<S, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const S& bound,
                                         const iterator& value) {
            return value == bound;
        }

        template<typename S,
                 enable_if_t<is_same_v<S, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const iterator& value,
                                         const S& bound) {
            return !(value == bound);
        }

        template<typename S,
                 enable_if_t<is_same_v<S, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const S& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    template<typename B1, typename B2>
    struct sentinel {
        using S1 = sentinel_t<B1>;
        using S2 = sentinel_t<B2>;
        S1 first_;
        S2 second_;

        constexpr sentinel() = default;
        constexpr sentinel(S1 first, S2 second)
            : first_(first), second_(second) {}
    };

    V1 first_;
    V2 second_;

public:
    zip_view() = default;
    constexpr zip_view(V1 first, V2 second)
        : first_(std::move(first)), second_(std::move(second)) {}

    constexpr V1 base1() const& { return first_; }
    constexpr V1 base1() && { return std::move(first_); }
    constexpr V2 base2() const& { return second_; }
    constexpr V2 base2() && { return std::move(second_); }

    constexpr auto begin() {
        return iterator<V1, V2>(ranges::begin(first_), ranges::begin(second_));
    }

    constexpr auto end() {
        return sentinel<V1, V2>(ranges::end(first_), ranges::end(second_));
    }

    constexpr auto begin() const
        requires input_range<const V1> && input_range<const V2> {
        return iterator<const V1, const V2>(
            ranges::begin(std::as_const(first_)),
            ranges::begin(std::as_const(second_)));
    }

    constexpr auto end() const
        requires input_range<const V1> && input_range<const V2> {
        return sentinel<const V1, const V2>(
            ranges::end(std::as_const(first_)),
            ranges::end(std::as_const(second_)));
    }

    constexpr auto size() requires sized_range<V1> && sized_range<V2> {
        using size_type = common_type_t<
            decltype(ranges::size(first_)),
            decltype(ranges::size(second_))>;
        const size_type first_size =
            static_cast<size_type>(ranges::size(first_));
        const size_type second_size =
            static_cast<size_type>(ranges::size(second_));
        return first_size < second_size ? first_size : second_size;
    }

    constexpr auto size() const
        requires sized_range<const V1> && sized_range<const V2> {
        using size_type = common_type_t<
            decltype(ranges::size(std::as_const(first_))),
            decltype(ranges::size(std::as_const(second_)))>;
        const size_type first_size = static_cast<size_type>(
            ranges::size(std::as_const(first_)));
        const size_type second_size = static_cast<size_type>(
            ranges::size(std::as_const(second_)));
        return first_size < second_size ? first_size : second_size;
    }
};

template<typename R1, typename R2>
zip_view(R1&&, R2&&) -> zip_view<owning_view<R1>, owning_view<R2>>;

template<typename V1, typename V2>
inline constexpr bool enable_borrowed_range<zip_view<V1, V2>> =
    borrowed_range<V1> && borrowed_range<V2>;

/* ???????????????????????????????????????????????????????????????
 * zip_transform_view - ?????? callable ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V1, typename V2, typename F>
    requires input_range<V1> && input_range<V2> && copy_constructible<F> &&
             regular_invocable<F&, range_reference_t<V1>, range_reference_t<V2>>
class zip_transform_view
    : public view_interface<zip_transform_view<V1, V2, F>> {
    using I1 = iterator_t<V1>;
    using I2 = iterator_t<V2>;
    using S1 = sentinel_t<V1>;
    using S2 = sentinel_t<V2>;

    template<typename B1, typename B2>
    struct sentinel;

    template<typename B1, typename B2, typename Function>
    struct iterator {
        using I1 = iterator_t<B1>;
        using I2 = iterator_t<B2>;
        I1 first_;
        I2 second_;
        Function* function_;

        using reference = decltype(std::invoke(
            declval<Function&>(), *declval<I1&>(), *declval<I2&>()));
        using value_type = remove_cvref_t<reference>;
        using difference_type = common_type_t<
            iter_difference_t<I1>, iter_difference_t<I2>>;
        using pointer = void;
        using iterator_category = input_iterator_tag;

        constexpr iterator() : first_(), second_(), function_(nullptr) {}
        constexpr iterator(I1 first, I2 second, Function* function)
            : first_(first), second_(second), function_(function) {}

        constexpr decltype(auto) operator*() const {
            return std::invoke(*function_, *first_, *second_);
        }

        constexpr iterator& operator++() {
            ++first_;
            ++second_;
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.first_ == right.first_ &&
                   left.second_ == right.second_;
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const iterator& value,
                                         const Bound& bound) {
            return value.first_ == bound.first_ ||
                   value.second_ == bound.second_;
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const Bound& bound,
                                         const iterator& value) {
            return value == bound;
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const iterator& value,
                                         const Bound& bound) {
            return !(value == bound);
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const Bound& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    template<typename B1, typename B2>
    struct sentinel {
        using S1 = sentinel_t<B1>;
        using S2 = sentinel_t<B2>;
        S1 first_;
        S2 second_;

        constexpr sentinel() = default;
        constexpr sentinel(S1 first, S2 second)
            : first_(first), second_(second) {}
    };

    V1 first_;
    V2 second_;
    F function_;

public:
    zip_transform_view() = default;
    constexpr zip_transform_view(V1 first, V2 second, F function)
        : first_(std::move(first)), second_(std::move(second)),
          function_(std::move(function)) {}

    constexpr V1 base1() const& { return first_; }
    constexpr V1 base1() && { return std::move(first_); }
    constexpr V2 base2() const& { return second_; }
    constexpr V2 base2() && { return std::move(second_); }
    constexpr F fun() const& { return function_; }
    constexpr F fun() && { return std::move(function_); }

    constexpr auto begin() {
        return iterator<V1, V2, F>(ranges::begin(first_), ranges::begin(second_),
                                   &function_);
    }

    constexpr auto end() {
        return sentinel<V1, V2>(ranges::end(first_), ranges::end(second_));
    }

    constexpr auto begin() const
        requires input_range<const V1> && input_range<const V2> &&
                 regular_invocable<const F&, range_reference_t<const V1>,
                                    range_reference_t<const V2>> {
        return iterator<const V1, const V2, const F>(
            ranges::begin(std::as_const(first_)),
            ranges::begin(std::as_const(second_)), &function_);
    }

    constexpr auto end() const
        requires input_range<const V1> && input_range<const V2> &&
                 regular_invocable<const F&, range_reference_t<const V1>,
                                    range_reference_t<const V2>> {
        return sentinel<const V1, const V2>(
            ranges::end(std::as_const(first_)),
            ranges::end(std::as_const(second_)));
    }

    constexpr auto size() requires sized_range<V1> && sized_range<V2> {
        using size_type = common_type_t<
            decltype(ranges::size(first_)),
            decltype(ranges::size(second_))>;
        const size_type first_size =
            static_cast<size_type>(ranges::size(first_));
        const size_type second_size =
            static_cast<size_type>(ranges::size(second_));
        return first_size < second_size ? first_size : second_size;
    }

    constexpr auto size() const
        requires sized_range<const V1> && sized_range<const V2> {
        using size_type = common_type_t<
            decltype(ranges::size(std::as_const(first_))),
            decltype(ranges::size(std::as_const(second_)))>;
        const size_type first_size = static_cast<size_type>(
            ranges::size(std::as_const(first_)));
        const size_type second_size = static_cast<size_type>(
            ranges::size(std::as_const(second_)));
        return first_size < second_size ? first_size : second_size;
    }
};

template<typename R1, typename R2, typename F>
zip_transform_view(R1&&, R2&&, F&&)
    -> zip_transform_view<owning_view<R1>, owning_view<R2>, decay_t<F>>;

template<typename V1, typename V2, typename F>
inline constexpr bool enable_borrowed_range<zip_transform_view<V1, V2, F>> =
    borrowed_range<V1> && borrowed_range<V2>;

#if __cplusplus > 202002L
/* The C++23 zip-transform adaptor accepts any number of ranges.  Keep the
 * original two-range view above for ABI/source compatibility and use this
 * tuple-backed cursor for the variadic form.  Iteration stops at the first
 * exhausted range, matching zip's shortest-range semantics. */
template<typename F, typename... Vs>
    requires (input_range<Vs> && ...) && copy_constructible<F> &&
             regular_invocable<F&, range_reference_t<Vs>...>
class zip_transform_view_variadic
    : public view_interface<zip_transform_view_variadic<F, Vs...>> {
    static_assert(sizeof...(Vs) >= 3u,
                  "variadic zip_transform requires at least three ranges");
    using iterator_tuple = tuple<iterator_t<Vs>...>;
    using sentinel_tuple = tuple<sentinel_t<Vs>...>;

    struct sentinel;

    struct iterator {
        iterator_tuple current_{};
        F* function_ = nullptr;

        constexpr iterator() = default;
        constexpr iterator(iterator_tuple current, F* function)
            : current_(std::move(current)), function_(function) {}

        template<size_t... Indices>
        constexpr decltype(auto) dereference(index_sequence<Indices...>) const {
            return std::invoke(*function_, *get<Indices>(current_)...);
        }

        template<size_t... Indices>
        constexpr void increment(index_sequence<Indices...>) {
            (++get<Indices>(current_), ...);
        }

        constexpr decltype(auto) operator*() const {
            return dereference(make_index_sequence<sizeof...(Vs)>{});
        }

        constexpr iterator& operator++() {
            increment(make_index_sequence<sizeof...(Vs)>{});
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.current_ == right.current_;
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        template<size_t... Indices>
        static constexpr bool at_end(const iterator& value,
                                     const sentinel& bound,
                                     index_sequence<Indices...>) {
            return ((get<Indices>(value.current_) ==
                     get<Indices>(bound.ends_)) || ...);
        }

        friend constexpr bool operator==(const iterator& value,
                                         const sentinel& bound) {
            return at_end(value, bound, make_index_sequence<sizeof...(Vs)>{});
        }

        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    struct sentinel {
        sentinel_tuple ends_{};
        constexpr sentinel() = default;
        constexpr explicit sentinel(sentinel_tuple ends)
            : ends_(std::move(ends)) {}
    };

    template<typename... Bases>
    struct const_sentinel {
        tuple<sentinel_t<const Bases>...> ends_{};
        constexpr const_sentinel() = default;
        constexpr explicit const_sentinel(
            tuple<sentinel_t<const Bases>...> ends)
            : ends_(std::move(ends)) {}
    };

    template<typename... Bases>
    struct const_iterator {
        using reference = decltype(std::invoke(
            std::declval<const F&>(),
            *std::declval<iterator_t<const Bases>&>()...));
        using value_type = remove_cvref_t<reference>;
        using difference_type = common_type_t<
            iter_difference_t<iterator_t<const Bases>>...>;
        using pointer = void;
        using iterator_category = input_iterator_tag;
        tuple<iterator_t<const Bases>...> current_{};
        const F* function_ = nullptr;

        constexpr const_iterator() = default;
        constexpr const_iterator(
            tuple<iterator_t<const Bases>...> current,
            const F* function)
            : current_(std::move(current)), function_(function) {}

        template<size_t... Indices>
        constexpr decltype(auto) dereference(index_sequence<Indices...>) const {
            return std::invoke(*function_, *get<Indices>(current_)...);
        }

        template<size_t... Indices>
        constexpr void increment(index_sequence<Indices...>) {
            (++get<Indices>(current_), ...);
        }

        constexpr decltype(auto) operator*() const {
            return dereference(make_index_sequence<sizeof...(Bases)>{});
        }

        constexpr const_iterator& operator++() {
            increment(make_index_sequence<sizeof...(Bases)>{});
            return *this;
        }

        constexpr const_iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const const_iterator& left,
                                         const const_iterator& right) {
            return left.current_ == right.current_;
        }

        friend constexpr bool operator!=(const const_iterator& left,
                                         const const_iterator& right) {
            return !(left == right);
        }

        template<size_t... Indices>
        static constexpr bool at_end(
            const const_iterator& value,
            const const_sentinel<Bases...>& bound,
            index_sequence<Indices...>) {
            return ((get<Indices>(value.current_) ==
                     get<Indices>(bound.ends_)) || ...);
        }

        friend constexpr bool operator==(
            const const_iterator& value,
            const const_sentinel<Bases...>& bound) {
            return at_end(value, bound,
                          make_index_sequence<sizeof...(Bases)>{});
        }

        friend constexpr bool operator==(
            const const_sentinel<Bases...>& bound,
            const const_iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(
            const const_iterator& value,
            const const_sentinel<Bases...>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(
            const const_sentinel<Bases...>& bound,
            const const_iterator& value) {
            return !(value == bound);
        }
    };

    tuple<Vs...> bases_;
    F function_;

    template<size_t... Indices>
    constexpr iterator begin_impl(index_sequence<Indices...>) {
        return iterator(
            iterator_tuple(ranges::begin(get<Indices>(bases_))...),
            &function_);
    }

    template<size_t... Indices>
    constexpr sentinel end_impl(index_sequence<Indices...>) {
        return sentinel(sentinel_tuple(ranges::end(get<Indices>(bases_))...));
    }

public:
    constexpr zip_transform_view_variadic() = default;
    constexpr zip_transform_view_variadic(tuple<Vs...> bases, F function)
        : bases_(std::move(bases)), function_(std::move(function)) {}

    constexpr auto begin() {
        return begin_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    constexpr auto end() {
        return end_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    template<size_t... Indices>
    constexpr auto begin_const_impl(index_sequence<Indices...>) const {
        return const_iterator<Vs...>(
            tuple<iterator_t<const Vs>...>(
                ranges::begin(std::as_const(get<Indices>(bases_)))...),
            &function_);
    }

    template<size_t... Indices>
    constexpr auto end_const_impl(index_sequence<Indices...>) const {
        return const_sentinel<Vs...>(
            tuple<sentinel_t<const Vs>...>(
                ranges::end(std::as_const(get<Indices>(bases_)))...));
    }

    constexpr auto begin() const
        requires (input_range<const Vs> && ...) &&
                 regular_invocable<const F&, range_reference_t<const Vs>...> {
        return begin_const_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    constexpr auto end() const
        requires (input_range<const Vs> && ...) &&
                 regular_invocable<const F&, range_reference_t<const Vs>...> {
        return end_const_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    template<size_t... Indices>
    constexpr auto size_impl(index_sequence<Indices...>) const {
        using size_type = common_type_t<
            decltype(ranges::size(get<Indices>(bases_)))...>;
        size_type result = static_cast<size_type>(ranges::size(get<0>(bases_)));
        ((static_cast<size_type>(ranges::size(get<Indices>(bases_))) < result
              ? result = static_cast<size_type>(
                    ranges::size(get<Indices>(bases_)))
              : result), ...);
        return result;
    }

    constexpr auto size() requires (sized_range<Vs> && ...) {
        return size_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    template<size_t... Indices>
    constexpr auto size_const_impl(index_sequence<Indices...>) const {
        using size_type = common_type_t<
            decltype(ranges::size(std::as_const(get<Indices>(bases_))))...>;
        size_type result = static_cast<size_type>(
            ranges::size(std::as_const(get<0>(bases_))));
        ((static_cast<size_type>(
              ranges::size(std::as_const(get<Indices>(bases_)))) < result
              ? result = static_cast<size_type>(
                    ranges::size(std::as_const(get<Indices>(bases_)))
                )
              : result), ...);
        return result;
    }

    constexpr auto size() const
        requires (sized_range<const Vs> && ...) {
        return size_const_impl(make_index_sequence<sizeof...(Vs)>{});
    }
};

template<typename F, typename... Vs>
inline constexpr bool enable_borrowed_range<
    zip_transform_view_variadic<F, Vs...>> = (borrowed_range<Vs> && ...);
#endif



/* zip and zip_transform share the same shortest-range cursor.  This identity
 * callable exposes the dereferenced tuple directly, preserving mutable
 * references without adding another iterator implementation. */
struct zip_view_variadic_identity {
    template<typename... Elements>
    constexpr auto operator()(Elements&&... elements) const {
        return tuple<decltype((elements))...>(
            std::forward<Elements>(elements)...);
    }
};

template<typename... Vs>
using zip_view_variadic =
    zip_transform_view_variadic<zip_view_variadic_identity, Vs...>;

/* ???????????????????????????????????????????????????????????????
 * pairwise_view - ????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires forward_range<V>
class pairwise_view : public view_interface<pairwise_view<V>> {
    template<typename Base>
    struct sentinel_impl {
        using S = sentinel_t<Base>;
        S end_;
        constexpr sentinel_impl() = default;
        constexpr explicit sentinel_impl(S end) : end_(end) {}
    };

    template<typename Base>
    struct iterator_impl {
        using I = iterator_t<Base>;
        using S = sentinel_t<Base>;
        using reference = pair<decltype(*declval<I&>()),
                               decltype(*declval<I&>())>;
        using value_type = pair<remove_cvref_t<decltype(*declval<I&>())>,
                                remove_cvref_t<decltype(*declval<I&>())>>;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using iterator_category = forward_iterator_tag;
        using iterator_concept = forward_iterator_tag;

        I first_{};
        I second_{};

        constexpr iterator_impl() = default;
        constexpr iterator_impl(I first, I second)
            : first_(first), second_(second) {}

        constexpr reference operator*() const {
            return reference(*first_, *second_);
        }

        constexpr iterator_impl& operator++() {
            first_ = second_;
            ++second_;
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& left,
                                         const iterator_impl& right) {
            return left.first_ == right.first_ &&
                   left.second_ == right.second_;
        }

        friend constexpr bool operator!=(const iterator_impl& left,
                                         const iterator_impl& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return value.second_ == bound.end_;
        }

        friend constexpr bool operator==(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator_impl& value,
                                         const sentinel_impl<Base>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel_impl<Base>& bound,
                                         const iterator_impl& value) {
            return !(value == bound);
        }
    };

    using iterator = iterator_impl<V>;
    using sentinel = sentinel_impl<V>;

    V base_;

public:
    pairwise_view() = default;
    constexpr explicit pairwise_view(V base) : base_(std::move(base)) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        auto first = ranges::begin(base_);
        auto last = ranges::end(base_);
        auto second = first;
        if (first != last) ++second;
        return iterator(first, second);
    }

    constexpr auto end() {
        return sentinel(ranges::end(base_));
    }

    constexpr auto size() requires sized_range<V> {
        const auto count = static_cast<size_t>(ranges::size(base_));
        return count == 0u ? 0u : count - 1u;
    }

    constexpr auto begin() const
        requires forward_range<const V> {
        using const_iterator = iterator_impl<const V>;
        auto first = ranges::begin(std::as_const(base_));
        auto last = ranges::end(std::as_const(base_));
        auto second = first;
        if (first != last) ++second;
        return const_iterator(first, second);
    }

    constexpr auto end() const
        requires forward_range<const V> {
        return sentinel_impl<const V>(ranges::end(std::as_const(base_)));
    }

    constexpr auto size() const requires sized_range<const V> {
        const auto count = static_cast<size_t>(ranges::size(
            std::as_const(base_)));
        return count == 0u ? 0u : count - 1u;
    }
};

template<typename R>
pairwise_view(R&&) -> pairwise_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<pairwise_view<V>> = borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * adjacent_view - N ????????? tuple ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V, size_t N, typename Sequence = make_index_sequence<N>>
struct adjacent_iterator_types;

template<typename V, size_t N, size_t... Indices>
struct adjacent_iterator_types<V, N, index_sequence<Indices...>> {
    using value_type = tuple<typename conditional<
        true, range_value_t<V>, integral_constant<size_t, Indices>>::type...>;
    using reference = tuple<typename conditional<
        true, range_reference_t<V>, integral_constant<size_t, Indices>>::type...>;
};

template<typename V, size_t N>
    requires forward_range<V>
class adjacent_view : public view_interface<adjacent_view<V, N>> {
    static_assert(N > 0, "views::adjacent requires a positive extent");

    using I = iterator_t<V>;
    using S = sentinel_t<V>;

    template<typename Base>
    struct sentinel;

    template<typename Base>
    struct iterator {
        using I = iterator_t<Base>;
        using S = sentinel_t<Base>;
        using iterator_types = adjacent_iterator_types<Base, N>;
        using value_type = typename iterator_types::value_type;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using reference = typename iterator_types::reference;
        using iterator_category = forward_iterator_tag;
        using iterator_concept = forward_iterator_tag;

        I positions_[N]{};
        bool empty_ = true;

        template<size_t... Indices>
        constexpr auto dereference(index_sequence<Indices...>) const {
            return tuple<decltype(*positions_[Indices])...>(
                *positions_[Indices]...);
        }

        constexpr iterator() = default;

        constexpr iterator(I first, S last) : positions_{}, empty_(false) {
            for (size_t index = 0; index < N; ++index)
                positions_[index] = first;
            if (first == last) {
                empty_ = true;
                return;
            }
            for (size_t index = 1; index < N; ++index) {
                positions_[index] = positions_[index - 1];
                ++positions_[index];
                if (positions_[index] == last) {
                    empty_ = true;
                    return;
                }
            }
        }

        constexpr auto operator*() const {
            return dereference(make_index_sequence<N>{});
        }

        constexpr iterator& operator++() {
            for (size_t index = 0; index + 1 < N; ++index)
                positions_[index] = positions_[index + 1];
            ++positions_[N - 1];
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            if (left.empty_ != right.empty_)
                return false;
            return left.empty_ || left.positions_[0] == right.positions_[0];
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator& value,
                                         const sentinel<Base>& bound) {
            return value.empty_ || value.positions_[N - 1] == bound.end_;
        }

        friend constexpr bool operator==(const sentinel<Base>& bound,
                                         const iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel<Base>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel<Base>& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    template<typename Base>
    struct sentinel {
        using S = sentinel_t<Base>;
        S end_;
        constexpr sentinel() = default;
        constexpr explicit sentinel(S end) : end_(end) {}
    };

    V base_;

public:
    adjacent_view() = default;
    constexpr explicit adjacent_view(V base) : base_(std::move(base)) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), ranges::end(base_));
    }

    constexpr auto end() {
        return sentinel<V>(ranges::end(base_));
    }

    constexpr auto begin() const
        requires forward_range<const V> {
        return iterator<const V>(ranges::begin(std::as_const(base_)),
                                 ranges::end(std::as_const(base_)));
    }

    constexpr auto end() const
        requires forward_range<const V> {
        return sentinel<const V>(ranges::end(std::as_const(base_)));
    }

    constexpr auto size() requires sized_range<V> {
        const auto count = static_cast<size_t>(ranges::size(base_));
        return count < N ? 0u : count - N + 1u;
    }

    constexpr auto size() const requires sized_range<const V> {
        const auto count = static_cast<size_t>(
            ranges::size(std::as_const(base_)));
        return count < N ? 0u : count - N + 1u;
    }
};

template<typename V, size_t N>
inline constexpr bool enable_borrowed_range<adjacent_view<V, N>> =
    borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * cartesian_product_view - ?????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename R>
concept cartesian_product_viewable = viewable_range<R> &&
    (is_lvalue_reference_v<R> || view<remove_cvref_t<R>> ||
     range<const remove_reference_t<R>>) &&
    input_range<R>;


template<typename V1, typename V2>
    requires input_range<V1> && input_range<V2>
class cartesian_product_view
    : public view_interface<cartesian_product_view<V1, V2>> {
    using I1 = iterator_t<V1>;
    using I2 = iterator_t<V2>;
    using S1 = sentinel_t<V1>;
    using S2 = sentinel_t<V2>;

    template<typename B1, typename B2>
    struct sentinel;

    template<typename B1, typename B2>
    struct iterator {
        using I1 = iterator_t<B1>;
        using I2 = iterator_t<B2>;
        using reference = pair<iter_reference_t<I1>, iter_reference_t<I2>>;
        using value_type = remove_cvref_t<reference>;
        using difference_type = common_type_t<iter_difference_t<I1>,
                                              iter_difference_t<I2>>;
        using pointer = void;
        using iterator_category = input_iterator_tag;
        I1 first_;
        I2 second_;
        I2 second_begin_;
        sentinel_t<B2> second_end_;
        bool empty_;

        constexpr iterator() : first_(), second_(), second_begin_(),
                                second_end_(), empty_(true) {}
        constexpr iterator(I1 first, I2 second, I2 second_begin,
                           sentinel_t<B2> second_end, bool empty)
            : first_(first), second_(second), second_begin_(second_begin),
              second_end_(second_end), empty_(empty) {}

        constexpr auto operator*() const {
            return std::pair<decltype(*first_), decltype(*second_)>(
                *first_, *second_);
        }

        constexpr iterator& operator++() {
            if (empty_) return *this;
            ++second_;
            if (second_ == second_end_) {
                second_ = second_begin_;
                ++first_;
            }
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.empty_ == right.empty_ &&
                   left.first_ == right.first_ &&
                   left.second_ == right.second_;
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const iterator& value,
                                         const Bound& bound) {
            return value.empty_ || value.first_ == bound.first_end_;
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator==(const Bound& bound,
                                         const iterator& value) {
            return value == bound;
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const iterator& value,
                                         const Bound& bound) {
            return !(value == bound);
        }

        template<typename Bound,
                 enable_if_t<is_same_v<Bound, sentinel<B1, B2>>, int> = 0>
        friend constexpr bool operator!=(const Bound& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    template<typename B1, typename B2>
    struct sentinel {
        using S1 = sentinel_t<B1>;
        S1 first_end_;
        constexpr sentinel() = default;
        constexpr explicit sentinel(S1 first_end) : first_end_(first_end) {}
    };

    V1 first_;
    V2 second_;

public:
    cartesian_product_view() = default;
    constexpr cartesian_product_view(V1 first, V2 second)
        : first_(std::move(first)), second_(std::move(second)) {}

    constexpr V1 base1() const& { return first_; }
    constexpr V1 base1() && { return std::move(first_); }
    constexpr V2 base2() const& { return second_; }
    constexpr V2 base2() && { return std::move(second_); }

    constexpr auto begin() {
        const auto first = ranges::begin(first_);
        const auto first_end = ranges::end(first_);
        const auto second = ranges::begin(second_);
        const auto second_end = ranges::end(second_);
        const bool empty = first == first_end || second == second_end;
        return iterator<V1, V2>(first, second, second, second_end, empty);
    }

    constexpr auto end() {
        return sentinel<V1, V2>(ranges::end(first_));
    }

    constexpr auto begin() const
        requires input_range<const V1> && input_range<const V2> {
        const auto first = ranges::begin(std::as_const(first_));
        const auto first_end = ranges::end(std::as_const(first_));
        const auto second = ranges::begin(std::as_const(second_));
        const auto second_end = ranges::end(std::as_const(second_));
        const bool empty = first == first_end || second == second_end;
        return iterator<const V1, const V2>(
            first, second, second, second_end, empty);
    }

    constexpr auto end() const
        requires input_range<const V1> && input_range<const V2> {
        return sentinel<const V1, const V2>(
            ranges::end(std::as_const(first_)));
    }

    constexpr auto size() requires sized_range<V1> && sized_range<V2> {
        using size_type = common_type_t<
            decltype(ranges::size(first_)),
            decltype(ranges::size(second_))>;
        const size_type first_size =
            static_cast<size_type>(ranges::size(first_));
        const size_type second_size =
            static_cast<size_type>(ranges::size(second_));
        if (first_size == 0 || second_size == 0) return size_type(0);
        const size_type maximum = ~size_type(0);
        if (first_size > maximum / second_size) return maximum;
        return first_size * second_size;
    }

    constexpr auto size() const
        requires sized_range<const V1> && sized_range<const V2> {
        using size_type = common_type_t<
            decltype(ranges::size(std::as_const(first_))),
            decltype(ranges::size(std::as_const(second_)))>;
        const size_type first_size = static_cast<size_type>(
            ranges::size(std::as_const(first_)));
        const size_type second_size = static_cast<size_type>(
            ranges::size(std::as_const(second_)));
        if (first_size == 0 || second_size == 0) return size_type(0);
        const size_type maximum = ~size_type(0);
        if (first_size > maximum / second_size) return maximum;
        return first_size * second_size;
    }
};

template<typename R1, typename R2>
cartesian_product_view(R1&&, R2&&)
    -> cartesian_product_view<owning_view<R1>, owning_view<R2>>;

template<typename V1, typename V2>
inline constexpr bool enable_borrowed_range<cartesian_product_view<V1, V2>> =
    borrowed_range<V1> && borrowed_range<V2>;

#if __cplusplus > 202002L
/* C++23 variadic cartesian product.  The iterator is an allocation-free
 * odometer: the last range advances first, then carries into earlier ranges.
 * The first range reaching its sentinel is the single end condition, while
 * every other cursor is reset to its begin cursor during a carry. */
template<typename... Vs>
    requires (input_range<Vs> && ...)
class cartesian_product_view_variadic
    : public view_interface<cartesian_product_view_variadic<Vs...>> {
    static_assert(sizeof...(Vs) >= 3u,
                  "variadic cartesian_product requires at least three ranges");
    using iterator_tuple = tuple<iterator_t<Vs>...>;
    using sentinel_tuple = tuple<sentinel_t<Vs>...>;

    struct sentinel;

    struct iterator {
        iterator_tuple current_{};
        iterator_tuple begins_{};
        sentinel_tuple ends_{};
        bool empty_ = true;

        constexpr iterator() = default;
        constexpr iterator(iterator_tuple current, iterator_tuple begins,
                           sentinel_tuple ends, bool empty)
            : current_(std::move(current)), begins_(std::move(begins)),
              ends_(std::move(ends)), empty_(empty) {}

        template<size_t... Indices>
        constexpr auto dereference(index_sequence<Indices...>) const {
            return tuple<decltype(*get<Indices>(current_))...>(
                *get<Indices>(current_)...);
        }

        template<size_t Index>
        constexpr void advance_index() {
            ++get<Index>(current_);
            if (get<Index>(current_) == get<Index>(ends_)) {
                if constexpr (Index == 0u) {
                    empty_ = true;
                } else {
                    get<Index>(current_) = get<Index>(begins_);
                    advance_index<Index - 1u>();
                }
            }
        }

        constexpr auto operator*() const {
            return dereference(make_index_sequence<sizeof...(Vs)>{});
        }

        constexpr iterator& operator++() {
            if (!empty_)
                advance_index<sizeof...(Vs) - 1u>();
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.empty_ == right.empty_ &&
                   (left.empty_ || left.current_ == right.current_);
        }

        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const iterator& value,
                                         const sentinel& bound) {
            (void)bound;
            return value.empty_;
        }

        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    struct sentinel {
        constexpr sentinel() = default;
    };

    template<typename... Bases>
    struct const_sentinel {
        constexpr const_sentinel() = default;
    };

    template<typename... Bases>
    struct const_iterator {
        using reference = tuple<iter_reference_t<iterator_t<const Bases>>...>;
        using value_type = remove_cvref_t<reference>;
        using difference_type = common_type_t<
            iter_difference_t<iterator_t<const Bases>>...>;
        using pointer = void;
        using iterator_category = input_iterator_tag;
        tuple<iterator_t<const Bases>...> current_{};
        tuple<iterator_t<const Bases>...> begins_{};
        tuple<sentinel_t<const Bases>...> ends_{};
        bool empty_ = true;

        constexpr const_iterator() = default;
        constexpr const_iterator(
            tuple<iterator_t<const Bases>...> current,
            tuple<iterator_t<const Bases>...> begins,
            tuple<sentinel_t<const Bases>...> ends,
            bool empty)
            : current_(std::move(current)), begins_(std::move(begins)),
              ends_(std::move(ends)), empty_(empty) {}

        template<size_t Index>
        constexpr void advance_index() {
            ++get<Index>(current_);
            if (get<Index>(current_) == get<Index>(ends_)) {
                if constexpr (Index == 0u) {
                    empty_ = true;
                } else {
                    get<Index>(current_) = get<Index>(begins_);
                    advance_index<Index - 1u>();
                }
            }
        }

        template<size_t... Indices>
        constexpr auto dereference(index_sequence<Indices...>) const {
            return tuple<decltype(*get<Indices>(current_))...>(
                *get<Indices>(current_)...);
        }

        constexpr auto operator*() const {
            return dereference(make_index_sequence<sizeof...(Bases)>{});
        }

        constexpr const_iterator& operator++() {
            if (!empty_)
                advance_index<sizeof...(Bases) - 1u>();
            return *this;
        }

        constexpr const_iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const const_iterator& left,
                                         const const_iterator& right) {
            return left.empty_ == right.empty_ &&
                   (left.empty_ || left.current_ == right.current_);
        }

        friend constexpr bool operator!=(const const_iterator& left,
                                         const const_iterator& right) {
            return !(left == right);
        }

        friend constexpr bool operator==(const const_iterator& value,
                                         const const_sentinel<Bases...>&) {
            return value.empty_;
        }

        friend constexpr bool operator==(const const_sentinel<Bases...>& bound,
                                         const const_iterator& value) {
            return value == bound;
        }

        friend constexpr bool operator!=(const const_iterator& value,
                                         const const_sentinel<Bases...>& bound) {
            return !(value == bound);
        }

        friend constexpr bool operator!=(const const_sentinel<Bases...>& bound,
                                         const const_iterator& value) {
            return !(value == bound);
        }
    };

    tuple<Vs...> bases_;

    template<size_t... Indices>
    constexpr iterator begin_impl(index_sequence<Indices...>) {
        auto begins = iterator_tuple(ranges::begin(get<Indices>(bases_))...);
        auto ends = sentinel_tuple(ranges::end(get<Indices>(bases_))...);
        auto current = begins;
        const bool empty = ((get<Indices>(current) == get<Indices>(ends)) || ...);
        return iterator(std::move(current), std::move(begins),
                        std::move(ends), empty);
    }

    template<size_t... Indices>
    constexpr sentinel end_impl(index_sequence<Indices...>) {
        (void)sizeof...(Indices);
        return sentinel();
    }

    using size_type = size_t;

    template<size_t Index>
    constexpr void multiply_size(size_type& result) {
        const size_type maximum = ~size_type(0);
        const size_type candidate = static_cast<size_type>(
            ranges::size(get<Index>(bases_)));
        if (result == 0 || candidate == 0) {
            result = 0;
        } else if (result > maximum / candidate) {
            result = maximum;
        } else {
            result *= candidate;
        }
        if constexpr (Index + 1u < sizeof...(Vs)) {
            if (result != maximum)
                multiply_size<Index + 1u>(result);
        }
    }

public:
    constexpr cartesian_product_view_variadic() = default;
    constexpr cartesian_product_view_variadic(tuple<Vs...> bases)
        : bases_(std::move(bases)) {}

    constexpr auto begin() {
        return begin_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    constexpr auto end() {
        return end_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    template<size_t... Indices>
    constexpr auto begin_const_impl(index_sequence<Indices...>) const {
        auto begins = tuple<iterator_t<const Vs>...>(
            ranges::begin(std::as_const(get<Indices>(bases_)))...);
        auto ends = tuple<sentinel_t<const Vs>...>(
            ranges::end(std::as_const(get<Indices>(bases_)))...);
        auto current = begins;
        const bool empty = ((get<Indices>(current) == get<Indices>(ends)) || ...);
        return const_iterator<Vs...>(std::move(current), std::move(begins),
                                     std::move(ends), empty);
    }

    template<size_t... Indices>
    constexpr const_sentinel<Vs...> end_const_impl(
        index_sequence<Indices...>) const {
        (void)sizeof...(Indices);
        return const_sentinel<Vs...>();
    }

    constexpr auto begin() const
        requires (input_range<const Vs> && ...) {
        return begin_const_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    constexpr auto end() const
        requires (input_range<const Vs> && ...) {
        return end_const_impl(make_index_sequence<sizeof...(Vs)>{});
    }

    constexpr auto size() requires (sized_range<Vs> && ...) {
        size_type result = 1;
        multiply_size<0u>(result);
        return result;
    }

    template<size_t Index>
    constexpr void multiply_size_const(size_type& result) const {
        const size_type maximum = ~size_type(0);
        const size_type candidate = static_cast<size_type>(
            ranges::size(std::as_const(get<Index>(bases_))));
        if (result == 0 || candidate == 0) {
            result = 0;
        } else if (result > maximum / candidate) {
            result = maximum;
        } else {
            result *= candidate;
        }
        if constexpr (Index + 1u < sizeof...(Vs)) {
            if (result != maximum)
                multiply_size_const<Index + 1u>(result);
        }
    }

    constexpr auto size() const
        requires (sized_range<const Vs> && ...) {
        size_type result = 1;
        multiply_size_const<0u>(result);
        return result;
    }
};

template<typename... Vs>
inline constexpr bool enable_borrowed_range<
    cartesian_product_view_variadic<Vs...>> = (borrowed_range<Vs> && ...);
#endif

/* ???????????????????????????????????????????????????????????????
 * enumerate_view - index ????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires input_range<V>
class enumerate_view : public view_interface<enumerate_view<V>> {
    template<typename Base>
    struct iterator {
        using base_iterator = iterator_t<Base>;
        using difference_type = range_difference_t<Base>;
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = pair<difference_type, range_value_t<Base>>;
        using pointer = void;
        using reference = pair<difference_type, range_reference_t<Base>>;

        base_iterator current_{};
        difference_type index_{};

        constexpr iterator() = default;
        constexpr iterator(base_iterator current, difference_type index)
            : current_(std::move(current)), index_(index) {}

        constexpr reference operator*() const {
            return reference(index_, *current_);
        }

        constexpr iterator& operator++() {
            ++current_;
            ++index_;
            return *this;
        }

        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        template<typename Other>
        friend constexpr bool operator==(const iterator& left,
                                         const iterator<Other>& right) {
            return left.current_ == right.current_;
        }

        template<typename Other>
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator<Other>& right) {
            return !(left == right);
        }
    };

    template<typename Base>
    struct sentinel {
        using base_sentinel = sentinel_t<Base>;
        base_sentinel end_{};

        constexpr sentinel() = default;
        constexpr explicit sentinel(base_sentinel end) : end_(std::move(end)) {}

        friend constexpr bool operator==(const iterator<Base>& value,
                                         const sentinel& bound) {
            return value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel& bound,
                                         const iterator<Base>& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator<Base>& value,
                                         const sentinel& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel& bound,
                                         const iterator<Base>& value) {
            return !(value == bound);
        }
    };

    V base_;

public:
    enumerate_view() = default;
    constexpr explicit enumerate_view(V base) : base_(std::move(base)) {}

    constexpr const V& base() const& { return base_; }
    constexpr V&& base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator<V>(ranges::begin(base_), range_difference_t<V>(0));
    }

    constexpr auto end() {
        return sentinel<V>(ranges::end(base_));
    }

    constexpr auto begin() const
        requires input_range<const V> {
        return iterator<const V>(
            ranges::begin(std::as_const(base_)), range_difference_t<const V>(0));
    }

    constexpr auto end() const
        requires input_range<const V> {
        return sentinel<const V>(ranges::end(std::as_const(base_)));
    }

    constexpr auto size() requires sized_range<V> {
        return ranges::size(base_);
    }

    constexpr auto size() const requires sized_range<const V> {
        return ranges::size(std::as_const(base_));
    }
};

template<typename R>
enumerate_view(R&&) -> enumerate_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<enumerate_view<V>> =
    borrowed_range<V>;

/* ???????????????????????????????????????????????????????????????
 * repeat_view - ???? bounded?unbounded ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename T, typename Bound = unreachable_sentinel_t>
    requires move_constructible<T> && is_object_v<T> &&
             same_as<T, remove_cv_t<T>> &&
             (same_as<Bound, unreachable_sentinel_t> || integral<Bound>)
class repeat_view : public view_interface<repeat_view<T, Bound>> {
    static constexpr bool bounded =
        !same_as<Bound, unreachable_sentinel_t>;
    using index_type = conditional_t<bounded, Bound, ptrdiff_t>;

    struct iterator {
        using iterator_category = input_iterator_tag;
        using iterator_concept = random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const T* value_;
        index_type index_;

        constexpr iterator() : value_(nullptr), index_(0) {}
        constexpr iterator(const T* value, index_type index)
            : value_(value), index_(index) {}

        constexpr const T& operator*() const { return *value_; }
        constexpr iterator& operator++() {
            ++index_;
            return *this;
        }
        constexpr iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        constexpr iterator& operator--() {
            --index_;
            return *this;
        }
        constexpr iterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }

        constexpr iterator& operator+=(difference_type offset) {
            if (offset >= 0) {
                index_ += static_cast<index_type>(offset);
            } else {
                index_ -= static_cast<index_type>(-(offset + 1)) + 1;
            }
            return *this;
        }
        constexpr iterator& operator-=(difference_type offset) {
            if (offset >= 0) {
                index_ -= static_cast<index_type>(offset);
            } else {
                index_ += static_cast<index_type>(-(offset + 1)) + 1;
            }
            return *this;
        }

        friend constexpr iterator operator+(iterator value,
                                            difference_type offset) {
            value += offset;
            return value;
        }
        friend constexpr iterator operator+(difference_type offset,
                                            iterator value) {
            value += offset;
            return value;
        }
        friend constexpr iterator operator-(iterator value,
                                            difference_type offset) {
            value -= offset;
            return value;
        }
        friend constexpr difference_type operator-(const iterator& left,
                                                   const iterator& right) {
            return left.index_ >= right.index_
                ? static_cast<difference_type>(left.index_ - right.index_)
                : -static_cast<difference_type>(right.index_ - left.index_);
        }

        constexpr const T& operator[](difference_type offset) const {
            return *(*this + offset);
        }

        friend constexpr bool operator==(const iterator& left,
                                         const iterator& right) {
            return left.value_ == right.value_ &&
                   left.index_ == right.index_;
        }
        friend constexpr bool operator!=(const iterator& left,
                                         const iterator& right) {
            return !(left == right);
        }
        friend constexpr bool operator<(const iterator& left,
                                        const iterator& right) {
            return left.index_ < right.index_;
        }
        friend constexpr bool operator>(const iterator& left,
                                        const iterator& right) {
            return right < left;
        }
        friend constexpr bool operator<=(const iterator& left,
                                         const iterator& right) {
            return !(right < left);
        }
        friend constexpr bool operator>=(const iterator& left,
                                         const iterator& right) {
            return !(left < right);
        }

    };

    T value_;
    Bound bound_;

public:
    repeat_view() requires default_initializable<T>
        : value_(), bound_() {}

    constexpr explicit repeat_view(T value)
        requires same_as<Bound, unreachable_sentinel_t>
        : value_(std::move(value)), bound_() {}

    constexpr repeat_view(T value, Bound bound)
        : value_(std::move(value)), bound_(bound) {
        if constexpr (signed_integral<Bound>) {
            if (bound < 0) rin_panic("negative repeat bound");
        }
    }

    constexpr const T& value() const noexcept { return value_; }

    constexpr auto begin() const {
        return iterator(&value_, 0);
    }

    constexpr auto end() const
        requires (!same_as<Bound, unreachable_sentinel_t>) {
        return iterator(&value_, static_cast<index_type>(bound_));
    }

    constexpr auto end() const
        requires same_as<Bound, unreachable_sentinel_t> {
        return unreachable_sentinel;
    }

    constexpr auto size() const
        requires (!same_as<Bound, unreachable_sentinel_t>) {
        if constexpr (signed_integral<Bound>) {
            return static_cast<make_unsigned_t<Bound>>(bound_);
        } else {
            return bound_;
        }
    }
};

template<typename T>
repeat_view(T) -> repeat_view<decay_t<T>, unreachable_sentinel_t>;

template<typename T, typename Bound>
repeat_view(T, Bound) -> repeat_view<decay_t<T>, decay_t<Bound>>;
#endif

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * chunk_view - ????????? subrange ???
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires input_range<V>
class chunk_view : public view_interface<chunk_view<V>> {
    V base_;
    size_t count_;

    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    template<typename BI, typename BS>
    struct sentinel_impl;

    template<typename BI, typename BS>
    struct iterator_impl {
        using iterator_category = input_iterator_tag;
        using value_type = subrange<BI, BS>;
        using difference_type = iter_difference_t<BI>;
        using pointer = void;
        using reference = value_type;

        BI current_;
        BS end_;
        size_t count_;
        bool empty_;

        iterator_impl() = default;
        constexpr iterator_impl(BI current, BS end, size_t count, bool empty)
            : current_(current), end_(end), count_(count), empty_(empty) {}

        constexpr value_type operator*() const {
            auto last = current_;
            size_t left = count_;
            while (left != 0u && last != end_) {
                ++last;
                --left;
            }
            return value_type(current_, last);
        }

        constexpr iterator_impl& operator++() {
            if (empty_) return *this;
            size_t left = count_;
            while (left != 0u && current_ != end_) {
                ++current_;
                --left;
            }
            return *this;
        }

        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        friend constexpr bool operator==(const iterator_impl& a,
                                         const iterator_impl& b) {
            return a.empty_ == b.empty_ &&
                   (a.empty_ || a.current_ == b.current_);
        }
        friend constexpr bool operator!=(const iterator_impl& a,
                                         const iterator_impl& b) {
            return !(a == b);
        }
        friend constexpr bool operator==(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>& s) {
            return i.empty_ || i.current_ == s.end_;
        }
        friend constexpr bool operator!=(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>& s) {
            return !(i == s);
        }
        friend constexpr bool operator==(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) {
            return i == s;
        }
        friend constexpr bool operator!=(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) {
            return !(i == s);
        }
    };

    template<typename BI, typename BS>
    struct sentinel_impl {
        BS end_;
        constexpr explicit sentinel_impl(BS end) : end_(end) {}
    };

public:
    chunk_view() = default;
    constexpr chunk_view(V base, size_t count)
        : base_(std::move(base)), count_(count) {}

    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        return iterator_impl<base_iterator, base_sentinel>(
            ranges::begin(base_), ranges::end(base_), count_, count_ == 0u);
    }

    constexpr auto end() {
        return sentinel_impl<base_iterator, base_sentinel>(ranges::end(base_));
    }

    constexpr auto begin() const requires input_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return iterator_impl<const_iterator, const_sentinel>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), count_, count_ == 0u);
    }

    constexpr auto end() const requires input_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return sentinel_impl<const_iterator, const_sentinel>(
            ranges::end(std::as_const(base_)));
    }

    constexpr size_t size() requires sized_range<V> {
        if (count_ == 0u) return 0u;
        const size_t n = static_cast<size_t>(ranges::size(base_));
        return n / count_ + (n % count_ != 0u ? 1u : 0u);
    }

    constexpr size_t size() const requires sized_range<const V> {
        if (count_ == 0u) return 0u;
        const size_t n = static_cast<size_t>(
            ranges::size(std::as_const(base_)));
        return n / count_ + (n % count_ != 0u ? 1u : 0u);
    }
};

template<typename R>
chunk_view(R&&, size_t) -> chunk_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<chunk_view<V>> =
    borrowed_range<V>;
#endif

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * slide_view - ??????? window ?????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires forward_range<V>
class slide_view : public view_interface<slide_view<V>> {
    V base_;
    size_t count_;
    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    template<typename BI, typename BS>
    struct sentinel_impl {
        BS end_;
        constexpr explicit sentinel_impl(BS end) : end_(end) {}
    };

    template<typename BI, typename BS>
    struct iterator_impl {
        using iterator_category = input_iterator_tag;
        using value_type = subrange<BI, BI>;
        using difference_type = iter_difference_t<BI>;
        using pointer = void;
        using reference = value_type;
        BI current_;
        BI next_;
        BS end_;
        bool empty_;

        iterator_impl() = default;
        constexpr iterator_impl(BI current, BI next, BS end, bool empty)
            : current_(current), next_(next), end_(end), empty_(empty) {}

        constexpr value_type operator*() const {
            return value_type(current_, next_);
        }
        constexpr iterator_impl& operator++() {
            if (empty_) return *this;
            if (next_ == end_) {
                empty_ = true;
                return *this;
            }
            ++current_;
            ++next_;
            return *this;
        }
        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend constexpr bool operator==(const iterator_impl& a,
                                         const iterator_impl& b) {
            return a.empty_ == b.empty_ &&
                   (a.empty_ || a.current_ == b.current_);
        }
        friend constexpr bool operator!=(const iterator_impl& a,
                                         const iterator_impl& b) { return !(a == b); }
        friend constexpr bool operator==(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>&) {
            return i.empty_;
        }
        friend constexpr bool operator!=(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>& s) { return !(i == s); }
        friend constexpr bool operator==(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) { return i == s; }
        friend constexpr bool operator!=(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) { return !(i == s); }
    };

public:
    slide_view() = default;
    constexpr slide_view(V base, size_t count)
        : base_(std::move(base)), count_(count) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }

    constexpr auto begin() {
        auto first = ranges::begin(base_);
        auto last = first;
        auto e = ranges::end(base_);
        size_t left = count_;
        while (left != 0u && last != e) { ++last; --left; }
        return iterator_impl<base_iterator, base_sentinel>(
            first, last, e, count_ == 0u || left != 0u);
    }
    constexpr auto end() {
        return sentinel_impl<base_iterator, base_sentinel>(ranges::end(base_));
    }

    constexpr auto begin() const
        requires forward_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        auto first = ranges::begin(std::as_const(base_));
        auto last = first;
        auto e = ranges::end(std::as_const(base_));
        size_t left = count_;
        while (left != 0u && last != e) { ++last; --left; }
        return iterator_impl<const_iterator, const_sentinel>(
            first, last, e, count_ == 0u || left != 0u);
    }

    constexpr auto end() const
        requires forward_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return sentinel_impl<const_iterator, const_sentinel>(
            ranges::end(std::as_const(base_)));
    }

    constexpr size_t size() requires sized_range<V> {
        const size_t n = static_cast<size_t>(ranges::size(base_));
        if (count_ == 0u || n < count_) return 0u;
        return n - count_ + 1u;
    }

    constexpr size_t size() const requires sized_range<const V> {
        const size_t n = static_cast<size_t>(
            ranges::size(std::as_const(base_)));
        if (count_ == 0u || n < count_) return 0u;
        return n - count_ + 1u;
    }
};

template<typename R>
slide_view(R&&, size_t) -> slide_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<slide_view<V>> =
    borrowed_range<V>;
#endif

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * stride_view - ????????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V>
    requires input_range<V>
class stride_view : public view_interface<stride_view<V>> {
    V base_;
    size_t step_;
    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    template<typename BI, typename BS>
    struct sentinel_impl {
        BS end_;
        constexpr explicit sentinel_impl(BS end) : end_(end) {}
    };

    template<typename BI, typename BS>
    struct iterator_impl {
        using iterator_category = input_iterator_tag;
        using value_type = iter_value_t<BI>;
        using difference_type = iter_difference_t<BI>;
        using pointer = typename iterator_traits<BI>::pointer;
        using reference = iter_reference_t<BI>;
        BI current_;
        BS end_;
        size_t step_;
        bool empty_;

        iterator_impl() = default;
        constexpr iterator_impl(BI current, BS end, size_t step, bool empty)
            : current_(current), end_(end), step_(step), empty_(empty) {}
        constexpr decltype(auto) operator*() const { return *current_; }
        constexpr iterator_impl& operator++() {
            if (empty_) return *this;
            size_t left = step_;
            while (left != 0u && current_ != end_) {
                ++current_;
                --left;
            }
            return *this;
        }
        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend constexpr bool operator==(const iterator_impl& a,
                                         const iterator_impl& b) {
            return a.empty_ == b.empty_ &&
                   (a.empty_ || a.current_ == b.current_);
        }
        friend constexpr bool operator!=(const iterator_impl& a,
                                         const iterator_impl& b) { return !(a == b); }
        friend constexpr bool operator==(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>& s) {
            return i.empty_ || i.current_ == s.end_;
        }
        friend constexpr bool operator!=(const iterator_impl& i,
                                         const sentinel_impl<BI, BS>& s) { return !(i == s); }
        friend constexpr bool operator==(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) { return i == s; }
        friend constexpr bool operator!=(const sentinel_impl<BI, BS>& s,
                                         const iterator_impl& i) { return !(i == s); }
    };

public:
    stride_view() = default;
    constexpr stride_view(V base, size_t step)
        : base_(std::move(base)), step_(step) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }
    constexpr auto begin() {
        return iterator_impl<base_iterator, base_sentinel>(
            ranges::begin(base_), ranges::end(base_), step_, step_ == 0u);
    }
    constexpr auto end() {
        return sentinel_impl<base_iterator, base_sentinel>(ranges::end(base_));
    }

    constexpr auto begin() const
        requires input_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return iterator_impl<const_iterator, const_sentinel>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), step_, step_ == 0u);
    }

    constexpr auto end() const
        requires input_range<const V> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return sentinel_impl<const_iterator, const_sentinel>(
            ranges::end(std::as_const(base_)));
    }

    constexpr size_t size() requires sized_range<V> {
        if (step_ == 0u) return 0u;
        const size_t n = static_cast<size_t>(ranges::size(base_));
        return n / step_ + (n % step_ != 0u ? 1u : 0u);
    }

    constexpr size_t size() const requires sized_range<const V> {
        if (step_ == 0u) return 0u;
        const size_t n = static_cast<size_t>(
            ranges::size(std::as_const(base_)));
        return n / step_ + (n % step_ != 0u ? 1u : 0u);
    }
};

template<typename R>
stride_view(R&&, size_t) -> stride_view<owning_view<R>>;

template<typename V>
inline constexpr bool enable_borrowed_range<stride_view<V>> =
    borrowed_range<V>;
#endif

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * chunk_by_view - ?? predicate ????????
 * ???????????????????????????????????????????????????????????????*/

template<typename V, typename Pred>
    requires forward_range<V> && copy_constructible<Pred> &&
             predicate<Pred&, range_reference_t<V>, range_reference_t<V>>
class chunk_by_view : public view_interface<chunk_by_view<V, Pred>> {
    V base_;
    Pred pred_;
    using base_iterator = iterator_t<V>;
    using base_sentinel = sentinel_t<V>;

    template<typename BI, typename BS, typename P>
    struct sentinel_impl {
        BS end_;
        constexpr explicit sentinel_impl(BS end) : end_(end) {}
    };

    template<typename BI, typename BS, typename P>
    struct iterator_impl {
        using iterator_category = input_iterator_tag;
        using value_type = subrange<BI, BI>;
        using difference_type = iter_difference_t<BI>;
        using pointer = void;
        using reference = value_type;
        BI current_;
        BS end_;
        P pred_;

        iterator_impl() = default;
        constexpr iterator_impl(BI current, BS end, P pred)
            : current_(current), end_(end), pred_(pred) {}

        constexpr BI boundary() const {
            auto last = current_;
            if (last == end_) return last;
            auto previous = last;
            ++last;
            while (last != end_ &&
                   std::invoke(*pred_, *previous, *last)) {
                previous = last;
                ++last;
            }
            return last;
        }

        constexpr value_type operator*() const {
            return value_type(current_, boundary());
        }
        constexpr iterator_impl& operator++() {
            current_ = boundary();
            return *this;
        }
        constexpr iterator_impl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        friend constexpr bool operator==(const iterator_impl& a,
                                         const iterator_impl& b) {
            return a.current_ == b.current_;
        }
        friend constexpr bool operator!=(const iterator_impl& a,
                                         const iterator_impl& b) { return !(a == b); }
        friend constexpr bool operator==(const iterator_impl& i,
                                         const sentinel_impl<BI, BS, P>& s) {
            return i.current_ == s.end_;
        }
        friend constexpr bool operator!=(const iterator_impl& i,
                                         const sentinel_impl<BI, BS, P>& s) { return !(i == s); }
        friend constexpr bool operator==(const sentinel_impl<BI, BS, P>& s,
                                         const iterator_impl& i) { return i == s; }
        friend constexpr bool operator!=(const sentinel_impl<BI, BS, P>& s,
                                         const iterator_impl& i) { return !(i == s); }
    };

public:
    chunk_by_view() = default;
    constexpr chunk_by_view(V base, Pred pred)
        : base_(std::move(base)), pred_(std::move(pred)) {}
    constexpr V base() const& { return base_; }
    constexpr V base() && { return std::move(base_); }
    constexpr auto begin() {
        return iterator_impl<base_iterator, base_sentinel, Pred*>(
            ranges::begin(base_), ranges::end(base_), &pred_);
    }
    constexpr auto end() {
        return sentinel_impl<base_iterator, base_sentinel, Pred*>(
            ranges::end(base_));
    }

    constexpr auto begin() const
        requires forward_range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>,
                           range_reference_t<const V>> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return iterator_impl<const_iterator, const_sentinel, const Pred*>(
            ranges::begin(std::as_const(base_)),
            ranges::end(std::as_const(base_)), &pred_);
    }

    constexpr auto end() const
        requires forward_range<const V> &&
                 predicate<const Pred&, range_reference_t<const V>,
                           range_reference_t<const V>> {
        using const_iterator = iterator_t<const V>;
        using const_sentinel = sentinel_t<const V>;
        return sentinel_impl<const_iterator, const_sentinel, const Pred*>(
            ranges::end(std::as_const(base_)));
    }
};

template<typename R, typename Pred>
chunk_by_view(R&&, Pred) -> chunk_by_view<owning_view<R>, Pred>;

template<typename V, typename Pred>
inline constexpr bool enable_borrowed_range<chunk_by_view<V, Pred>> =
    borrowed_range<V>;
#endif

/* ???????????????????????????????????????????????????????????????
 * views ???? - ??????????
 * ???????????????????????????????????????????????????????????????*/

namespace views {

/* empty */
template<typename T>
inline constexpr empty_view<T> empty{};

/* single */
template<typename T>
concept single_viewable = copy_constructible<decay_t<T>> &&
    is_object_v<decay_t<T>>;

struct single_fn {
    template<typename T>
        requires single_viewable<T>
    constexpr auto operator()(T&& value) const {
        return single_view<decay_t<T>>(std::forward<T>(value));
    }
};
inline constexpr single_fn single{};

/* counted - bounded iterator view */
struct counted_fn {
    template<input_or_output_iterator I>
    constexpr auto operator()(I iterator,
                              iter_difference_t<I> count) const {
        if constexpr (signed_integral<iter_difference_t<I>>) {
            if (count < 0) {
                rin_panic("negative counted view length");
            }
        }
        using counted_type = counted_iterator<I>;
        return subrange<counted_type, default_sentinel_t>(
            counted_type(std::move(iterator), count),
            default_sentinel);
    }
};
inline constexpr counted_fn counted{};

/* iota */
struct iota_fn {
    template<typename W>
        requires detail::iota_value<W>
    constexpr auto operator()(W value) const {
        return iota_view<W>(std::move(value));
    }

    template<typename W, typename Bound>
        requires detail::iota_value<W> && detail::iota_bounded<W, Bound>
    constexpr auto operator()(W value, Bound bound) const {
        return iota_view<W, Bound>(std::move(value), std::move(bound));
    }
};
inline constexpr iota_fn iota{};

/* all - ????????? */
template<typename R>
concept all_viewable = viewable_range<R>;

struct all_fn {
    template<typename R>
        requires all_viewable<R>
    constexpr auto operator()(R&& r) const {
        if constexpr (view<decay_t<R>>) {
            return std::forward<R>(r);
        } else if constexpr (is_lvalue_reference_v<R>) {
            return ref_view(r);
        } else {
            return owning_view(std::forward<R>(r));
        }
    }
};
inline constexpr all_fn all{};

template<typename R>
    requires all_viewable<R>
constexpr auto operator|(R&& r, all_fn) {
    return all(std::forward<R>(r));
}

/* join */
struct join_adaptor {
    template<typename R>
        requires join_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return join_view<decltype(view)>(std::move(view));
    }
};

struct join_fn {
    template<typename R>
        requires join_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return join_view<decltype(view)>(std::move(view));
    }

    constexpr auto operator()() const { return join_adaptor{}; }
};
inline constexpr join_fn join{};

template<typename R>
    requires join_viewable<R>
constexpr auto operator|(R&& range, join_adaptor) {
    return join(std::forward<R>(range));
}

#if __cplusplus > 202002L
/* join_with */
template<typename D>
struct join_with_adaptor {
    D delimiter_;

    template<typename R>
        requires join_with_factory_viewable<R, D>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return join_with_view<decltype(view), D>(
            std::move(view), delimiter_);
    }
};

struct join_with_fn {
    template<typename R, typename D>
        requires join_with_factory_viewable<R, D>
    constexpr auto operator()(R&& range, D&& delimiter) const {
        auto view = all(std::forward<R>(range));
        return join_with_view<decltype(view), decay_t<D>>(
            std::move(view), std::forward<D>(delimiter));
    }

    template<typename D>
    constexpr auto operator()(D&& delimiter) const {
        return join_with_adaptor<decay_t<D>>{
            std::forward<D>(delimiter)};
    }
};
inline constexpr join_with_fn join_with{};

template<typename R, typename D>
    requires join_with_factory_viewable<R, D>
constexpr auto operator|(R&& range, join_with_adaptor<D> adaptor) {
    return adaptor(std::forward<R>(range));
}
#endif

#if __cplusplus > 202002L
/* concat */
template<typename R1, typename R2>
concept concat_pair_viewable = viewable_range<R1> &&
    viewable_range<R2> &&
    common_reference_with<
        range_reference_t<decltype(all(declval<R1>()))>,
        range_reference_t<decltype(all(declval<R2>()))>>;

template<typename... Rs>
struct concat_pairwise_impl;

template<>
struct concat_pairwise_impl<> : true_type {};

template<typename R>
struct concat_pairwise_impl<R> : true_type {};

template<typename R, typename... Rest>
struct concat_pairwise_impl<R, Rest...>
    : bool_constant<(concat_pair_viewable<R, Rest> && ...) &&
                    concat_pairwise_impl<Rest...>::value> {};

template<typename... Rs>
concept concat_factory_viewable =
    concat_pairwise_impl<Rs...>::value;

template<typename V, typename R>
constexpr auto concat_append(V base, R&& range) {
    auto view = all(std::forward<R>(range));
    return concat_view<V, decltype(view)>(
        std::move(base), std::move(view));
}

template<typename V, typename R, typename... Rs>
constexpr auto concat_append(V base, R&& range, Rs&&... rest) {
    auto next = concat_append(std::move(base), std::forward<R>(range));
    return concat_append(std::move(next), std::forward<Rs>(rest)...);
}

struct concat_fn {
    template<typename R1, typename R2>
        requires concat_factory_viewable<R1, R2>
    constexpr auto operator()(R1&& first, R2&& second) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        return concat_view<decltype(first_view), decltype(second_view)>(
            std::move(first_view), std::move(second_view));
    }

    template<typename R1, typename R2, typename... Rs>
        requires concat_factory_viewable<R1, R2, Rs...> &&
                 (sizeof...(Rs) > 0)
    constexpr auto operator()(R1&& first, R2&& second,
                              Rs&&... rest) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        auto base = concat_view<decltype(first_view), decltype(second_view)>(
            std::move(first_view), std::move(second_view));
        return concat_append(std::move(base), std::forward<Rs>(rest)...);
    }
};
inline constexpr concat_fn concat{};
#endif





template<typename R>
concept split_viewable = viewable_range<R> && forward_range<R>;

template<typename R, typename T>
concept split_value_viewable = split_viewable<R> &&
    requires(range_reference_t<remove_reference_t<R>> value,
             const T& delimiter) {
        value == delimiter;
    };

template<typename R, typename T>
concept lazy_split_value_viewable = viewable_range<R> && input_range<R> &&
    requires(range_reference_t<remove_reference_t<R>> value,
             const T& delimiter) {
        value == delimiter;
    };

template<typename R, typename P>
concept lazy_split_pattern_viewable = viewable_range<R> && input_range<R> &&
    range<P> && forward_range<P> &&
    requires(range_reference_t<remove_reference_t<R>> value,
             range_reference_t<remove_reference_t<P>> pattern) {
        value == pattern;
    };

template<typename R, typename P>
concept split_pattern_viewable = split_viewable<R> && range<P> &&
    forward_range<P> &&
    requires(range_reference_t<remove_reference_t<R>> value,
             range_reference_t<remove_reference_t<P>> pattern) {
        value == pattern;
    };

template<typename R>
concept pairwise_viewable = viewable_range<R> && forward_range<R>;

template<typename R>
concept adjacent_viewable = viewable_range<R> && forward_range<R>;

#if __cplusplus > 202002L
template<typename R>
concept as_rvalue_viewable = viewable_range<R> && input_range<R>;
#endif

template<typename P>
struct split_pattern_adaptor {
    P pattern_;
    template<typename R>
        requires split_pattern_viewable<R, P>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return split_pattern_view<decltype(base), P>(std::move(base), pattern_);
    }
};

template<typename T>
struct split_adaptor {
    T delimiter_;
    template<typename R>
        requires split_value_viewable<R, T>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return split_view<decltype(base), T>(
            std::move(base), delimiter_);
    }
};

struct split_fn {
    template<typename R, typename P>
        requires split_pattern_viewable<R, P>
    constexpr auto operator()(R&& range, P&& pattern) const {
        auto base = all(std::forward<R>(range));
        auto pat = all(std::forward<P>(pattern));
        return split_pattern_view<decltype(base), decltype(pat)>(std::move(base), std::move(pat));
    }

    template<typename R, typename T>
        requires split_value_viewable<R, remove_cvref_t<T>> &&
                 (!range<remove_cvref_t<T>> )
    constexpr auto operator()(R&& range, T&& delimiter) const {
        auto base = all(std::forward<R>(range));
        return split_view<decltype(base), remove_cvref_t<T>>(
            std::move(base), std::forward<T>(delimiter));
    }
    template<typename P>
        requires range<remove_reference_t<P>>
    constexpr auto operator()(P&& pattern) const {
        auto pat = all(std::forward<P>(pattern));
        return split_pattern_adaptor<decltype(pat)>{std::move(pat)};
    }

    template<typename T>
        requires (!range<remove_cvref_t<T>>)
    constexpr auto operator()(T&& delimiter) const {
        return split_adaptor<decay_t<T>>{std::forward<T>(delimiter)};
    }
};
inline constexpr split_fn split{};

template<typename R, typename P>
    requires split_pattern_viewable<R, P>
constexpr auto operator|(R&& range, split_pattern_adaptor<P> adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R, typename T>
    requires split_value_viewable<R, T>
constexpr auto operator|(R&& range, split_adaptor<T> adaptor) {
    return adaptor(std::forward<R>(range));
}

/* lazy_split (C++23): the existing split cursor already performs lazy
 * boundary discovery and supports value or forward-range patterns.  Expose
 * that cursor under the standard lazy_split CPO while preserving its
 * allocation-free subrange publication. */
template<typename V, typename T>
using lazy_split_value_view = split_view<V, T>;

template<typename V, typename P>
using lazy_split_pattern_view = split_pattern_view<V, P>;

template<typename T>
struct lazy_split_adaptor {
    T delimiter_;

    template<typename R>
        requires lazy_split_value_viewable<R, T>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        if constexpr (forward_range<decltype(base)>) {
            return split_view<decltype(base), T>(
                std::move(base), delimiter_);
        } else {
            return lazy_split_input_view<decltype(base), T>(
                std::move(base), delimiter_);
        }
    }
};

template<typename P>
struct lazy_split_pattern_adaptor {
    P pattern_;

    template<typename R>
        requires lazy_split_pattern_viewable<R, P>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        if constexpr (forward_range<decltype(base)>) {
            return split_pattern_view<decltype(base), P>(
                std::move(base), pattern_);
        } else {
            return lazy_split_input_pattern_view<decltype(base), P>(
                std::move(base), pattern_);
        }
    }
};

struct lazy_split_fn {
    template<typename R, typename P>
        requires lazy_split_pattern_viewable<R, P>
    constexpr auto operator()(R&& range, P&& pattern) const {
        auto base = all(std::forward<R>(range));
        auto pat = all(std::forward<P>(pattern));
        if constexpr (forward_range<decltype(base)>) {
            return lazy_split_pattern_view<decltype(base), decltype(pat)>(
                std::move(base), std::move(pat));
        } else {
            return lazy_split_input_pattern_view<decltype(base), decltype(pat)>(
                std::move(base), std::move(pat));
        }
    }

    template<typename R, typename T>
        requires lazy_split_value_viewable<R, remove_cvref_t<T>> &&
                 (!range<remove_cvref_t<T>>)
    constexpr auto operator()(R&& range, T&& delimiter) const {
        auto base = all(std::forward<R>(range));
        using base_type = decltype(base);
        using delimiter_type = remove_cvref_t<T>;
        if constexpr (forward_range<base_type>) {
            return split_view<base_type, delimiter_type>(
                std::move(base), std::forward<T>(delimiter));
        } else {
            return lazy_split_input_view<base_type, delimiter_type>(
                std::move(base), std::forward<T>(delimiter));
        }
    }

    template<typename P>
        requires range<remove_reference_t<P>>
    constexpr auto operator()(P&& pattern) const {
        auto pat = all(std::forward<P>(pattern));
        return lazy_split_pattern_adaptor<decltype(pat)>{std::move(pat)};
    }

    template<typename T>
        requires (!range<remove_cvref_t<T>>)
    constexpr auto operator()(T&& delimiter) const {
        return lazy_split_adaptor<decay_t<T>>{std::forward<T>(delimiter)};
    }
};
inline constexpr lazy_split_fn lazy_split{};

template<typename R, typename T>
    requires lazy_split_value_viewable<R, T>
constexpr auto operator|(R&& range, lazy_split_adaptor<T> adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R, typename P>
    requires lazy_split_pattern_viewable<R, P>
constexpr auto operator|(R&& range, lazy_split_pattern_adaptor<P> adaptor) {
    return adaptor(std::forward<R>(range));
}

/* adjacent<N> */
template<size_t N, typename R>
concept adjacent_extent_viewable = (N > 0) && adjacent_viewable<R>;

template<size_t N>
struct adjacent_adaptor {
    template<typename R>
        requires adjacent_extent_viewable<N, R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return adjacent_view<decltype(view), N>(std::move(view));
    }
};

template<size_t N>
struct adjacent_fn {
    template<typename R>
        requires adjacent_extent_viewable<N, R>
    constexpr auto operator()(R&& range) const {
        return adjacent_adaptor<N>{}(std::forward<R>(range));
    }

    constexpr auto operator()() const requires (N > 0) {
        return adjacent_adaptor<N>{};
    }
};

template<size_t N>
inline constexpr adjacent_fn<N> adjacent{};

template<typename R, size_t N>
    requires adjacent_extent_viewable<N, R>
constexpr auto operator|(R&& range, adjacent_adaptor<N> adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename F, typename Tuple, typename Sequence>
struct adjacent_transform_invocable_impl : false_type {};

template<typename F, typename Tuple, size_t... Indices>
struct adjacent_transform_invocable_impl<
    F, Tuple, index_sequence<Indices...>>
    : bool_constant<requires(F& function, Tuple& tuple) {
          std::invoke(function, std::get<Indices>(tuple)...);
      }> {};

template<typename F, typename Tuple>
concept adjacent_transform_invocable =
    adjacent_transform_invocable_impl<
        F, Tuple,
        make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>>::value;

template<size_t N, typename R, typename F>
concept adjacent_transform_viewable =
    (N > 0) && adjacent_viewable<R> &&
    copy_constructible<decay_t<F>> &&
    adjacent_transform_invocable<
        decay_t<F>, range_reference_t<decltype(adjacent<N>(declval<R>()))>>;

template<typename R, typename F>
concept transform_viewable = viewable_range<R> && input_range<R> &&
    copy_constructible<decay_t<F>> &&
    regular_invocable<decay_t<F>&, range_reference_t<remove_reference_t<R>>>;

template<typename R, typename Pred>
concept filter_viewable = viewable_range<R> && input_range<R> &&
    copy_constructible<decay_t<Pred>> &&
    predicate<decay_t<Pred>&, range_reference_t<remove_reference_t<R>>>;
/* transform */
template<typename F>
struct transform_adaptor {
    F func_;

    template<typename R>
        requires transform_viewable<R, F>
    constexpr auto operator()(R&& r) const {
        return transform_view(all(std::forward<R>(r)), func_);
    }
};

struct transform_fn {
    template<typename R, typename F>
        requires transform_viewable<R, F>
    constexpr auto operator()(R&& r, F&& f) const {
        return transform_view(all(std::forward<R>(r)), std::forward<F>(f));
    }

    template<typename F>
        requires copy_constructible<decay_t<F>>
    constexpr auto operator()(F&& f) const {
        return transform_adaptor<decay_t<F>>{std::forward<F>(f)};
    }
};
inline constexpr transform_fn transform{};

#if __cplusplus > 202002L
/* adjacent_transform<N> composes the adjacent reference tuple with a
 * callable.  Keep the tuple as a forwarding value so a callable can observe
 * the same lvalue references exposed by adjacent<N>, while transform_view
 * remains responsible for lazy iteration and sentinel handling. */
template<typename F>
struct adjacent_transform_callable {
    F function_;

    template<typename Tuple>
    constexpr decltype(auto) operator()(Tuple&& tuple) {
        return std::apply(function_, std::forward<Tuple>(tuple));
    }

    template<typename Tuple>
    constexpr decltype(auto) operator()(Tuple&& tuple) const {
        return std::apply(function_, std::forward<Tuple>(tuple));
    }
};

template<typename V, typename F>
    requires range<V> && copy_constructible<F> &&
             adjacent_transform_invocable<F, range_reference_t<V>>
class adjacent_transform_view
    : public view_interface<adjacent_transform_view<V, F>> {
    template<typename Base>
    struct sentinel;

    template<typename Base, typename Function>
    struct iterator {
        using I = iterator_t<Base>;
        using S = sentinel_t<Base>;
        I current_;
        Function* function_;
        using iterator_category = forward_iterator_tag;
        using iterator_concept = forward_iterator_tag;
        using difference_type = range_difference_t<Base>;
        using pointer = void;
        using reference = decltype(std::apply(
            std::declval<Function&>(), *std::declval<I&>()));
        using value_type = remove_cvref_t<reference>;

        constexpr iterator() = default;
        constexpr iterator(I current, Function* function)
            : current_(current), function_(function) {}
        constexpr decltype(auto) operator*() const {
            return std::apply(*function_, *current_);
        }
        constexpr iterator& operator++() { ++current_; return *this; }
        constexpr iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        friend constexpr bool operator==(const iterator& value,
                                         const iterator& other) {
            return value.current_ == other.current_;
        }
        friend constexpr bool operator!=(const iterator& value,
                                         const iterator& other) {
            return !(value == other);
        }
        friend constexpr bool operator==(const iterator& value,
                                         const sentinel<Base>& bound) {
            return value.current_ == bound.end_;
        }
        friend constexpr bool operator==(const sentinel<Base>& bound,
                                         const iterator& value) {
            return value == bound;
        }
        friend constexpr bool operator!=(const iterator& value,
                                         const sentinel<Base>& bound) {
            return !(value == bound);
        }
        friend constexpr bool operator!=(const sentinel<Base>& bound,
                                         const iterator& value) {
            return !(value == bound);
        }
    };

    template<typename Base>
    struct sentinel {
        using S = sentinel_t<Base>;
        S end_;
        constexpr explicit sentinel(S end) : end_(end) {}
    };

    V base_;
    F function_;

public:
    constexpr adjacent_transform_view(V base, F function)
        : base_(std::move(base)), function_(std::move(function)) {}

    constexpr auto begin() {
        return iterator<V, F>{ranges::begin(base_), &function_};
    }
    constexpr auto end() {
        return sentinel<V>{ranges::end(base_)};
    }

    constexpr auto begin() const
        requires range<const V> &&
                 adjacent_transform_invocable<
                     const F, range_reference_t<const V>> {
        return iterator<const V, const F>{
            ranges::begin(std::as_const(base_)), &function_};
    }
    constexpr auto end() const
        requires range<const V> &&
                 adjacent_transform_invocable<
                     const F, range_reference_t<const V>> {
        return sentinel<const V>{ranges::end(std::as_const(base_))};
    }

    template<typename X = V>
    constexpr auto size() -> decltype(std::declval<X&>().size()) {
        return ranges::size(base_);
    }

    constexpr auto size() const requires sized_range<const V> {
        return ranges::size(std::as_const(base_));
    }
};

template<size_t N, typename F>
struct adjacent_transform_adaptor {
    F function_;

    template<typename R>
        requires adjacent_transform_viewable<N, R, F>
    constexpr auto operator()(R&& range) const {
        auto adjacent_range = adjacent<N>(std::forward<R>(range));
        return adjacent_transform_view<decltype(adjacent_range), F>(
            std::move(adjacent_range), function_);
    }
};

template<size_t N>
struct adjacent_transform_fn {
    template<typename R, typename F>
        requires adjacent_transform_viewable<N, R, F>
    constexpr auto operator()(R&& range, F&& function) const {
        return adjacent_transform_adaptor<N, decay_t<F>>{
            std::forward<F>(function)}(std::forward<R>(range));
    }

    template<typename F>
        requires (N > 0) && copy_constructible<decay_t<F>>
    constexpr auto operator()(F&& function) const {
        return adjacent_transform_adaptor<N, decay_t<F>>{
            std::forward<F>(function)};
    }
};

template<size_t N>
inline constexpr adjacent_transform_fn<N> adjacent_transform{};

template<typename R, size_t N, typename F>
    requires adjacent_transform_viewable<N, R, F>
constexpr auto operator|(
    R&& range, adjacent_transform_adaptor<N, F> adaptor) {
    return adaptor(std::forward<R>(range));
}
#endif

#if __cplusplus > 202002L
struct as_rvalue_fn {
    template<typename R>
        requires as_rvalue_viewable<R>
    constexpr auto operator()(R&& range) const {
        return transform(std::forward<R>(range),
            [](auto&& value) -> decltype(auto) {
                return std::move(value);
            });
    }
};
inline constexpr as_rvalue_fn as_rvalue{};

template<typename R>
    requires as_rvalue_viewable<R>
constexpr auto operator|(R&& range, as_rvalue_fn) {
    return as_rvalue(std::forward<R>(range));
}
#endif

template<size_t Index, typename R>
concept elements_viewable = viewable_range<R> && input_range<R> && requires {
    std::get<Index>(declval<range_reference_t<remove_reference_t<R>>>());
};

/* elements / keys / values: lazy tuple-like projections built on the
 * existing transform_view so mutable pair fields remain writable. */
template<size_t Index>
struct elements_fn {
    template<typename R>
        requires elements_viewable<Index, R>
    constexpr auto operator()(R&& range) const {
        return transform(std::forward<R>(range),
            [](auto&& value) -> decltype(auto) {
                return std::get<Index>(
                    std::forward<decltype(value)>(value));
            });
    }
};

template<size_t Index>
inline constexpr elements_fn<Index> elements{};

inline constexpr elements_fn<0> keys{};
inline constexpr elements_fn<1> values{};
template<typename R, size_t Index>
    requires elements_viewable<Index, R>
constexpr auto operator|(R&& range, elements_fn<Index>) {
    return elements<Index>(std::forward<R>(range));
}

/* filter */
template<typename Pred>
struct filter_adaptor {
    Pred pred_;

    template<typename R>
        requires filter_viewable<R, Pred>
    constexpr auto operator()(R&& r) const {
        return filter_view(all(std::forward<R>(r)), pred_);
    }
};

struct filter_fn {
    template<typename R, typename Pred>
        requires filter_viewable<R, Pred>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return filter_view(all(std::forward<R>(r)), std::forward<Pred>(pred));
    }

    template<typename Pred>
        requires copy_constructible<decay_t<Pred>>
    constexpr auto operator()(Pred&& pred) const {
        return filter_adaptor<decay_t<Pred>>{std::forward<Pred>(pred)};
    }
};
inline constexpr filter_fn filter{};

template<typename R, typename F>
    requires transform_viewable<R, F>
constexpr auto operator|(R&& range, transform_adaptor<F> adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R, typename Pred>
    requires filter_viewable<R, Pred>
constexpr auto operator|(R&& range, filter_adaptor<Pred> adaptor) {
    return adaptor(std::forward<R>(range));
}
/* take_while / drop_while */
/* common */
template<typename R>
concept common_viewable = viewable_range<R> && input_range<R>;
struct common_fn {
    template<typename R>
        requires common_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return common_view<decltype(base)>(std::move(base));
    }
};
inline constexpr common_fn common{};
template<typename R>
    requires common_viewable<R>
constexpr auto operator|(R&& range, common_fn) {
    return common(std::forward<R>(range));
}

template<typename R, typename Pred>
concept take_while_viewable = viewable_range<R> && input_range<R> &&
    copy_constructible<decay_t<Pred>> &&
    predicate<decay_t<Pred>&, range_reference_t<remove_reference_t<R>>>;

template<typename R, typename Pred>
concept drop_while_viewable = viewable_range<R> && input_range<R> &&
    copy_constructible<decay_t<Pred>> &&
    predicate<decay_t<Pred>&, range_reference_t<remove_reference_t<R>>>;

template<typename Pred>
struct take_while_adaptor_for {
    Pred pred_;

    template<typename R>
        requires take_while_viewable<R, Pred>
    constexpr auto operator()(R&& r) const {
        return take_while_view(all(std::forward<R>(r)), pred_);
    }
};

struct take_while_fn {
    template<typename R, typename Pred>
        requires take_while_viewable<R, Pred>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return take_while_view(all(std::forward<R>(r)),
                               std::forward<Pred>(pred));
    }

    template<typename Pred>
        requires copy_constructible<decay_t<Pred>>
    constexpr auto operator()(Pred&& pred) const {
        return take_while_adaptor_for<decay_t<Pred>>{
            std::forward<Pred>(pred)};
    }
};
inline constexpr take_while_fn take_while{};

template<typename Pred>
struct drop_while_adaptor {
    Pred pred_;

    template<typename R>
        requires drop_while_viewable<R, Pred>
    constexpr auto operator()(R&& r) const {
        return drop_while_view(all(std::forward<R>(r)), pred_);
    }
};

struct drop_while_fn {
    template<typename R, typename Pred>
        requires drop_while_viewable<R, Pred>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return drop_while_view(all(std::forward<R>(r)),
                               std::forward<Pred>(pred));
    }

    template<typename Pred>
        requires copy_constructible<decay_t<Pred>>
    constexpr auto operator()(Pred&& pred) const {
        return drop_while_adaptor<decay_t<Pred>>{
            std::forward<Pred>(pred)};
    }
};
inline constexpr drop_while_fn drop_while{};

template<typename R, typename Pred>
    requires take_while_viewable<R, Pred>
constexpr auto operator|(R&& range, take_while_adaptor_for<Pred> adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R, typename Pred>
    requires drop_while_viewable<R, Pred>
constexpr auto operator|(R&& range, drop_while_adaptor<Pred> adaptor) {
    return adaptor(std::forward<R>(range));
}

/* take */
template<typename R>
concept take_viewable = viewable_range<R> && input_range<R>;

template<typename R>
concept drop_viewable = viewable_range<R> && input_range<R>;

template<typename R>
concept reverse_viewable = viewable_range<R> &&
    bidirectional_range<R> && common_range<R>;

struct take_adaptor {
    ptrdiff_t count_;

    template<typename R>
        requires take_viewable<R>
    constexpr auto operator()(R&& r) const {
        auto base = all(std::forward<R>(r));
        return take_view<decltype(base)>(std::move(base), count_);
    }
};

struct take_fn {
    template<typename R>
        requires take_viewable<R>
    constexpr auto operator()(R&& r, ptrdiff_t count) const {
        auto base = all(std::forward<R>(r));
        return take_view<decltype(base)>(std::move(base), count);
    }

    constexpr auto operator()(ptrdiff_t count) const {
        return take_adaptor{count};
    }
};
inline constexpr take_fn take{};

/* drop */
struct drop_adaptor {
    ptrdiff_t count_;

    template<typename R>
        requires drop_viewable<R>
    constexpr auto operator()(R&& r) const {
        auto base = all(std::forward<R>(r));
        return drop_view<decltype(base)>(std::move(base), count_);
    }
};

struct drop_fn {
    template<typename R>
        requires drop_viewable<R>
    constexpr auto operator()(R&& r, ptrdiff_t count) const {
        auto base = all(std::forward<R>(r));
        return drop_view<decltype(base)>(std::move(base), count);
    }

    constexpr auto operator()(ptrdiff_t count) const {
        return drop_adaptor{count};
    }
};
inline constexpr drop_fn drop{};

/* reverse */
struct reverse_fn {
    template<typename R>
        requires reverse_viewable<R>
    constexpr auto operator()(R&& r) const {
        auto base = all(std::forward<R>(r));
        return reverse_view<decltype(base)>(std::move(base));
    }
};
inline constexpr reverse_fn reverse{};

template<typename R>
    requires take_viewable<R>
constexpr auto operator|(R&& range, take_adaptor adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R>
    requires drop_viewable<R>
constexpr auto operator|(R&& range, drop_adaptor adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename R>
    requires reverse_viewable<R>
constexpr auto operator|(R&& range, reverse_fn) {
    return reverse(std::forward<R>(range));
}

#if __cplusplus > 202002L
template<typename R>
concept as_const_viewable = viewable_range<R> && input_range<R>;

struct as_const_fn {
    template<typename R>
        requires as_const_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return as_const_view<decltype(view)>(std::move(view));
    }
};
inline constexpr as_const_fn as_const{};

template<typename R>
    requires as_const_viewable<R>
constexpr auto operator|(R&& range, as_const_fn) {
    return as_const(std::forward<R>(range));
}

template<typename R>
concept zip_viewable = viewable_range<R> && input_range<R>;

template<typename F, typename... Rs>
concept zip_transform_viewable = (zip_viewable<Rs> && ...) &&
    copy_constructible<decay_t<F>> &&
    regular_invocable<decay_t<F>&, range_reference_t<remove_reference_t<Rs>>...>;


struct zip_fn {
    template<typename R1, typename R2>
        requires zip_viewable<R1> && zip_viewable<R2>
    constexpr auto operator()(R1&& first, R2&& second) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        return zip_view<decltype(first_view), decltype(second_view)>(
            std::move(first_view), std::move(second_view));
    }

#if __cplusplus > 202002L
    template<typename R1, typename R2, typename... Rs>
        requires (zip_viewable<R1> && zip_viewable<R2> && (zip_viewable<Rs> && ...))
    constexpr auto operator()(R1&& first, R2&& second,
                              Rs&&... rest) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        auto rest_views = tuple<decltype(all(std::forward<Rs>(rest)))...>(
            all(std::forward<Rs>(rest))...);
        return zip_view_variadic<
            decltype(first_view), decltype(second_view),
            decltype(all(std::forward<Rs>(rest)))...>(
            tuple_cat(
                tuple<decltype(first_view), decltype(second_view)>(
                    std::move(first_view), std::move(second_view)),
                std::move(rest_views)),
            zip_view_variadic_identity{});
    }
#endif
};
inline constexpr zip_fn zip{};

struct zip_transform_fn {
    template<typename F, typename R1, typename R2>
        requires zip_transform_viewable<F, R1, R2>
    constexpr auto operator()(F&& function, R1&& first, R2&& second) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        return zip_transform_view<decltype(first_view), decltype(second_view),
                                  decay_t<F>>(
            std::move(first_view), std::move(second_view),
            std::forward<F>(function));
    }

#if __cplusplus > 202002L
    template<typename F, typename R1, typename R2, typename... Rs>
        requires zip_transform_viewable<F, R1, R2, Rs...>
    constexpr auto operator()(F&& function, R1&& first, R2&& second,
                              Rs&&... rest) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        auto rest_views = tuple<decltype(all(std::forward<Rs>(rest)))...>(
            all(std::forward<Rs>(rest))...);
        return zip_transform_view_variadic<
            decay_t<F>, decltype(first_view), decltype(second_view),
            decltype(all(std::forward<Rs>(rest)))...>(
            tuple_cat(
                tuple<decltype(first_view), decltype(second_view)>(
                    std::move(first_view), std::move(second_view)),
                std::move(rest_views)),
            std::forward<F>(function));
    }
#endif
};
inline constexpr zip_transform_fn zip_transform{};

struct pairwise_fn {
    template<typename R>
        requires pairwise_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return pairwise_view<decltype(view)>(std::move(view));
    }
};
inline constexpr pairwise_fn pairwise{};

template<typename R>
    requires pairwise_viewable<R>
constexpr auto operator|(R&& range, pairwise_fn) {
    return pairwise(std::forward<R>(range));
}

struct cartesian_product_fn {
    template<typename R1, typename R2>
        requires cartesian_product_viewable<R1> && cartesian_product_viewable<R2>
    constexpr auto operator()(R1&& first, R2&& second) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        return cartesian_product_view<decltype(first_view), decltype(second_view)>(
            std::move(first_view), std::move(second_view));
    }

#if __cplusplus > 202002L
    template<typename R1, typename R2, typename... Rs>
        requires (cartesian_product_viewable<R1> && cartesian_product_viewable<R2> && (cartesian_product_viewable<Rs> && ...))
    constexpr auto operator()(R1&& first, R2&& second,
                              Rs&&... rest) const {
        auto first_view = all(std::forward<R1>(first));
        auto second_view = all(std::forward<R2>(second));
        auto rest_views = tuple<decltype(all(std::forward<Rs>(rest)))...>(
            all(std::forward<Rs>(rest))...);
        return cartesian_product_view_variadic<
            decltype(first_view), decltype(second_view),
            decltype(all(std::forward<Rs>(rest)))...>(
            tuple_cat(
                tuple<decltype(first_view), decltype(second_view)>(
                    std::move(first_view), std::move(second_view)),
                std::move(rest_views)));
    }
#endif
};
inline constexpr cartesian_product_fn cartesian_product{};

#if __cplusplus > 202002L
template<typename R>
concept enumerate_viewable = viewable_range<R> && input_range<R>;


struct enumerate_fn {
    template<typename R>
        requires enumerate_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto view = all(std::forward<R>(range));
        return enumerate_view<decltype(view)>(std::move(view));
    }
};
inline constexpr enumerate_fn enumerate{};

template<typename R>
    requires enumerate_viewable<R>
constexpr auto operator|(R&& range, enumerate_fn) {
    return enumerate(std::forward<R>(range));
}

#if __cplusplus > 202002L
template<typename T>
concept repeat_value_viewable = move_constructible<decay_t<T>> && is_object_v<decay_t<T>>;

template<typename T, typename Count>
concept repeat_bounded_viewable = repeat_value_viewable<T> && integral<remove_cvref_t<Count>>;

struct repeat_fn {
    template<typename T>
        requires repeat_value_viewable<T>
    constexpr auto operator()(T&& value) const {
        return repeat_view<decay_t<T>, unreachable_sentinel_t>(
            std::forward<T>(value));
    }

    template<typename T, typename Count>
        requires repeat_bounded_viewable<T, Count>
    constexpr auto operator()(T&& value, Count count) const {
        using count_type = remove_cvref_t<Count>;
        if constexpr (signed_integral<count_type>) {
            if (count < 0) {
                rin_panic("negative repeat bound");
            }
        }
        return repeat_view<decay_t<T>, count_type>(
            std::forward<T>(value), static_cast<count_type>(count));
    }
};
inline constexpr repeat_fn repeat{};
#endif
#endif

#if __cplusplus > 202002L
template<typename R>
concept chunk_viewable = viewable_range<R> && input_range<R>;

template<typename R>
concept slide_viewable = viewable_range<R> && forward_range<R>;

template<typename R>
concept stride_viewable = viewable_range<R> && input_range<R>;

template<typename R, typename Pred>
concept chunk_by_viewable = viewable_range<R> && forward_range<R> &&
    copy_constructible<remove_cvref_t<Pred>> &&
    predicate<remove_cvref_t<Pred>&, range_reference_t<R>, range_reference_t<R>>;

struct chunk_adaptor {
    size_t count_;

    template<typename R>
        requires chunk_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return chunk_view<decltype(base)>(std::move(base), count_);
    }
};

struct chunk_fn {
    template<typename R>
        requires chunk_viewable<R>
    constexpr auto operator()(R&& range, size_t count) const {
        auto base = all(std::forward<R>(range));
        return chunk_view<decltype(base)>(std::move(base), count);
    }

    constexpr auto operator()(size_t count) const {
        return chunk_adaptor{count};
    }
};
inline constexpr chunk_fn chunk{};

template<typename R>
    requires chunk_viewable<R>
constexpr auto operator|(R&& range, chunk_adaptor adaptor) {
    return adaptor(std::forward<R>(range));
}

struct slide_adaptor {
    size_t count_;
    template<typename R>
        requires slide_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return slide_view<decltype(base)>(std::move(base), count_);
    }
};

struct slide_fn {
    template<typename R>
        requires slide_viewable<R>
    constexpr auto operator()(R&& range, size_t count) const {
        auto base = all(std::forward<R>(range));
        return slide_view<decltype(base)>(std::move(base), count);
    }
    constexpr auto operator()(size_t count) const {
        return slide_adaptor{count};
    }
};
inline constexpr slide_fn slide{};

template<typename R>
    requires slide_viewable<R>
constexpr auto operator|(R&& range, slide_adaptor adaptor) {
    return adaptor(std::forward<R>(range));
}

struct stride_adaptor {
    size_t step_;
    template<typename R>
        requires stride_viewable<R>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return stride_view<decltype(base)>(std::move(base), step_);
    }
};

struct stride_fn {
    template<typename R>
        requires stride_viewable<R>
    constexpr auto operator()(R&& range, size_t step) const {
        auto base = all(std::forward<R>(range));
        return stride_view<decltype(base)>(std::move(base), step);
    }
    constexpr auto operator()(size_t step) const {
        return stride_adaptor{step};
    }
};
inline constexpr stride_fn stride{};

template<typename R>
    requires stride_viewable<R>
constexpr auto operator|(R&& range, stride_adaptor adaptor) {
    return adaptor(std::forward<R>(range));
}

template<typename Pred>
struct chunk_by_adaptor {
    Pred pred_;
    template<typename R>
        requires chunk_by_viewable<R, Pred>
    constexpr auto operator()(R&& range) const {
        auto base = all(std::forward<R>(range));
        return chunk_by_view<decltype(base), Pred>(
            std::move(base), pred_);
    }
};

struct chunk_by_fn {
    template<typename R, typename Pred>
        requires chunk_by_viewable<R, Pred>
    constexpr auto operator()(R&& range, Pred&& pred) const {
        auto base = all(std::forward<R>(range));
        return chunk_by_view<decltype(base), decay_t<Pred>>(
            std::move(base), std::forward<Pred>(pred));
    }
    template<typename Pred>
        requires copy_constructible<decay_t<Pred>>
    constexpr auto operator()(Pred&& pred) const {
        return chunk_by_adaptor<decay_t<Pred>>{std::forward<Pred>(pred)};
    }
};
inline constexpr chunk_by_fn chunk_by{};

template<typename R, typename Pred>
    requires chunk_by_viewable<R, Pred>
constexpr auto operator|(R&& range, chunk_by_adaptor<Pred> adaptor) {
    return adaptor(std::forward<R>(range));
}
#endif
#endif

} /* namespace views */

#if __cplusplus > 202002L
template<typename V, typename F>
inline constexpr bool enable_borrowed_range<views::adjacent_transform_view<V, F>> =
    borrowed_range<V>;
#endif

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * ranges::to - iterator?sentinel ?? container ???
 * ???????????????????????????????????????????????????????????????*/

namespace detail {

template<typename Container, typename R>
concept container_from_range_constructible =
    requires(R&& range) {
        Container(from_range, std::forward<R>(range));
    } ||
    requires(R&& range) {
        Container(ranges::begin(range), ranges::end(range));
    } ||
    (default_initializable<Container> &&
     (requires(Container& container, R&& range) {
          container.append_range(std::forward<R>(range));
      } ||
      requires(Container& container, R&& range) {
          container.push_back(*ranges::begin(range));
      }));

template<typename Container, typename R, typename... Args>
concept container_from_range_with_args =
    requires(R&& range, Args&&... args) {
        Container(from_range, std::forward<R>(range),
                  std::forward<Args>(args)...);
    } ||
    requires(R&& range, Args&&... args) {
        Container(ranges::begin(range), ranges::end(range),
                  std::forward<Args>(args)...);
    } ||
    (requires(Args&&... args) {
         Container(std::forward<Args>(args)...);
     } &&
     (requires(Container& container, R&& range) {
          container.append_range(std::forward<R>(range));
      } ||
      requires(Container& container, R&& range) {
          container.push_back(*ranges::begin(range));
      }));

} /* namespace detail */

template<typename Container>
struct to_fn {
    template<typename R>
        requires range<R&> &&
                 detail::container_from_range_constructible<Container, R>
    constexpr Container operator()(R&& range) const {
        return std::detail::make_container_from_range<Container>(
            std::forward<R>(range));
    }

    template<typename R, typename... Args>
        requires (sizeof...(Args) > 0) && range<R&> &&
                 detail::container_from_range_with_args<Container, R, Args...>
    constexpr Container operator()(R&& range, Args&&... args) const {
        auto arguments = std::tuple<Args&&...>(std::forward<Args>(args)...);
        return std::detail::make_container_from_range_args<Container>(
            std::forward<R>(range), std::move(arguments),
            make_index_sequence<sizeof...(Args)>());
    }
};
template<typename Container>
inline constexpr to_fn<Container> to{};

template<typename R, typename Container>
    requires range<R&>
constexpr auto operator|(R&& range, to_fn<Container> adaptor) {
    return adaptor(std::forward<R>(range));
}
#endif

/* ???????????????????????????????????????????????????????????????
 * identity - ?????????????
 * ???????????????????????????????????????????????????????????????*/

struct identity {
    template<typename T>
    constexpr T&& operator()(T&& t) const noexcept {
        return std::forward<T>(t);
    }
};

namespace detail {

/* `ranges::find` uses an equality comparison between the projected iterator
 * value and the searched value.  Keep the expression SFINAE-friendly so that
 * an invalid projection/value pair removes the CPO overload instead of
 * failing while instantiating its loop body. */
template<typename Proj, typename I, typename T, typename = void>
struct find_projection_valid : false_type {};

template<typename Proj, typename I, typename T>
struct find_projection_valid<
    Proj, I, T,
    void_t<
        decltype(std::invoke(declval<Proj&>(), *declval<I&>())),
        decltype(std::invoke(declval<Proj&>(), *declval<I&>()) ==
                 declval<const T&>())>>
    : is_convertible<decltype(
          std::invoke(declval<Proj&>(), *declval<I&>()) ==
          declval<const T&>()), bool> {};

} /* namespace detail */

/* ???????????????????????????????????????????????????????????????
 * find / contains range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct find_fn {
    template<typename I, typename S, typename T, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::find_projection_valid<Proj, I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, const T& value,
                           Proj proj = {}) const {
        while (first != last &&
               !static_cast<bool>(std::invoke(proj, *first) == value)) {
            ++first;
        }
        return first;
    }

    template<typename R, typename T, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::find_projection_valid<
                                 Proj, iterator_t<R>, T>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, const T& value,
                                                 Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        auto result = (*this)(first, last, value, std::move(proj));
        if constexpr (borrowed_range<R>) {
            return result;
        } else {
            return dangling(result);
        }
    }
};

inline constexpr find_fn find{};

namespace detail {

template<typename Pred, typename Proj1, typename Proj2, typename I1,
         typename I2, typename = void>
struct binary_predicate_valid : false_type {};

template<typename Pred, typename Proj1, typename Proj2, typename I1,
         typename I2>
struct binary_predicate_valid<
    Pred, Proj1, Proj2, I1, I2,
    void_t<decltype(std::invoke(
        declval<Pred&>(),
        std::invoke(declval<Proj1&>(), *declval<I1&>()),
        std::invoke(declval<Proj2&>(), *declval<I2&>())))>>
    : is_convertible<decltype(std::invoke(
          declval<Pred&>(),
          std::invoke(declval<Proj1&>(), *declval<I1&>()),
          std::invoke(declval<Proj2&>(), *declval<I2&>()))), bool> {};

template<typename Pred, typename Proj, typename I, typename = void>
struct unary_predicate_valid : false_type {};

template<typename Pred, typename Proj, typename I>
struct unary_predicate_valid<
    Pred, Proj, I,
    void_t<decltype(std::invoke(
        declval<Pred&>(), std::invoke(declval<Proj&>(), *declval<I&>())))>>
    : is_convertible<decltype(std::invoke(
          declval<Pred&>(),
          std::invoke(declval<Proj&>(), *declval<I&>()))), bool> {};

template<typename Pred, typename Proj, typename I, typename T,
         typename = void>
struct value_predicate_valid : false_type {};

template<typename Pred, typename Proj, typename I, typename T>
struct value_predicate_valid<
    Pred, Proj, I, T,
    void_t<decltype(std::invoke(
               declval<Pred&>(),
               std::invoke(declval<Proj&>(), *declval<I&>()),
               declval<const T&>())),
           decltype(std::invoke(
               declval<Pred&>(), declval<const T&>(),
               std::invoke(declval<Proj&>(), *declval<I&>())))>>
    : integral_constant<bool,
          is_convertible<decltype(std::invoke(
              declval<Pred&>(),
              std::invoke(declval<Proj&>(), *declval<I&>()),
              declval<const T&>())), bool>::value &&
          is_convertible<decltype(std::invoke(
              declval<Pred&>(), declval<const T&>(),
              std::invoke(declval<Proj&>(), *declval<I&>()))), bool>::value> {};

} /* namespace detail */

/* ???????????????????????????????????????????????????????????????
 * equal / count range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct equal_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Pred pred = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj1, *first1),
                    std::invoke(proj2, *first2)))) {
                return false;
            }
            ++first1;
            ++first2;
        }
        return first1 == last1 && first2 == last2;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<range<R1> && range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
    }
};

inline constexpr equal_fn equal{};

struct count_fn {
    template<typename I, typename S, typename T, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::find_projection_valid<Proj, I, T>::value,
                         int> = 0>
    constexpr iter_difference_t<I> operator()(I first, S last,
                                              const T& value,
                                              Proj proj = {}) const {
        iter_difference_t<I> result = 0;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(proj, *first) == value))
                ++result;
        }
        return result;
    }

    template<typename R, typename T, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::find_projection_valid<
                                 Proj, iterator_t<R>, T>::value,
                         int> = 0>
    constexpr iter_difference_t<iterator_t<R>> operator()(R&& range,
                                                          const T& value,
                                                          Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range), value,
                       std::move(proj));
    }
};

inline constexpr count_fn count{};

struct count_if_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr iter_difference_t<I> operator()(I first, S last, Pred pred,
                                              Proj proj = {}) const {
        iter_difference_t<I> result = 0;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                ++result;
            }
        }
        return result;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr iter_difference_t<iterator_t<R>> operator()(R&& range, Pred pred,
                                                          Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range),
                       std::move(pred), std::move(proj));
    }
};

inline constexpr count_if_fn count_if{};

struct find_if_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           Proj proj = {}) const {
        while (first != last && !static_cast<bool>(std::invoke(
                   pred, std::invoke(proj, *first)))) {
            ++first;
        }
        return first;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        auto result = (*this)(first, last, std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return result;
        } else {
            return dangling(result);
        }
    }
};

inline constexpr find_if_fn find_if{};

struct find_if_not_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           Proj proj = {}) const {
        while (first != last && static_cast<bool>(std::invoke(
                   pred, std::invoke(proj, *first)))) {
            ++first;
        }
        return first;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        auto result = (*this)(first, last, std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return result;
        } else {
            return dangling(result);
        }
    }
};

inline constexpr find_if_not_fn find_if_not{};

#if __cplusplus > 202002L
/* ???????????????????????????????????????????????????????????????
 * find_last range algorithms (C++23)
 * ???????????????????????????????????????????????????????????????*/

struct find_last_if_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        I result = end;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                result = first;
            }
        }
        return result;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr find_last_if_fn find_last_if{};

struct find_last_if_not_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        I result = end;
        for (; first != last; ++first) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                result = first;
            }
        }
        return result;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr find_last_if_not_fn find_last_if_not{};

struct find_last_fn {
    template<typename I, typename S, typename T, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::find_projection_valid<Proj, I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, const T& value,
                           Proj proj = {}) const {
        return find_last_if_fn{}(first, last,
            [&](const auto& element) {
                return static_cast<bool>(std::invoke(proj, element) == value);
            });
    }

    template<typename R, typename T, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::find_projection_valid<Proj,
                                 iterator_t<R>, T>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, const T& value,
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range), value,
                              std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr find_last_fn find_last{};

struct contains_subrange_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Pred pred = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        if (first2 == last2) return true;
        I1 end1 = first1;
        while (end1 != last1) ++end1;
        for (I1 candidate = first1; candidate != end1; ++candidate) {
            I1 current1 = candidate;
            I2 current2 = first2;
            while (current1 != end1 && current2 != last2 &&
                   static_cast<bool>(std::invoke(
                       pred, std::invoke(proj1, *current1),
                       std::invoke(proj2, *current2)))) {
                ++current1;
                ++current2;
            }
            if (current2 == last2) return true;
            if (current1 == end1) return false;
        }
        return false;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<forward_range<R1> && forward_range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
    }
};

inline constexpr contains_subrange_fn contains_subrange{};
#endif

/* ???????????????????????????????????????????????????????????????
 * search / adjacent-find / permutation range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct adjacent_find_fn {
    template<typename I, typename S, typename Pred = equal_to<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred = {},
                           Proj proj = {}) const {
        if (first == last) return first;
        I next = first;
        ++next;
        for (; next != last; ++next, ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first),
                    std::invoke(proj, *next)))) {
                return first;
            }
        }
        return next;
    }

    template<typename R, typename Pred = equal_to<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr adjacent_find_fn adjacent_find{};

struct search_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr subrange<I1> operator()(I1 first1, S1 last1, I2 first2,
                                      S2 last2, Pred pred = {},
                                      Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        I1 end1 = first1;
        while (end1 != last1) ++end1;
        I2 end2 = first2;
        while (end2 != last2) ++end2;
        if (first2 == end2) return {first1, first1};

        for (I1 candidate = first1; candidate != end1; ++candidate) {
            I1 current1 = candidate;
            I2 current2 = first2;
            while (current1 != end1 && current2 != end2 &&
                   static_cast<bool>(std::invoke(
                       pred, std::invoke(proj1, *current1),
                       std::invoke(proj2, *current2)))) {
                ++current1;
                ++current2;
            }
            if (current2 == end2) return {candidate, current1};
            if (current1 == end1) return {end1, end1};
        }
        return {end1, end1};
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<forward_range<R1> && forward_range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr auto operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto result = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              std::move(pred), std::move(proj1),
                              std::move(proj2));
        if constexpr (borrowed_range<R1>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr search_fn search{};

struct search_n_fn {
    template<typename I, typename S, typename Size, typename T,
             typename Pred = equal_to<>, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::value_predicate_valid<
                                 Pred, Proj, I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Size count, const T& value,
                           Pred pred = {}, Proj proj = {}) const {
        if (count <= 0) return first;
        for (I candidate = first; candidate != last; ++candidate) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *candidate), value))) {
                continue;
            }
            I current = candidate;
            Size remaining = count;
            while (current != last && static_cast<bool>(std::invoke(
                       pred, std::invoke(proj, *current), value))) {
                --remaining;
                if (remaining == 0) return candidate;
                ++current;
            }
        }
        I end = first;
        while (end != last) ++end;
        return end;
    }

    template<typename R, typename Size, typename T,
             typename Pred = equal_to<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::value_predicate_valid<
                                 Pred, Proj, iterator_t<R>, T>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Size count,
                                                 const T& value,
                                                 Pred pred = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range), count,
                              value, std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr search_n_fn search_n{};

struct find_end_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr subrange<I1> operator()(I1 first1, S1 last1, I2 first2,
                                      S2 last2, Pred pred = {},
                                      Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        I1 end1 = first1;
        while (end1 != last1) ++end1;
        I2 end2 = first2;
        while (end2 != last2) ++end2;
        if (first2 == end2) return {end1, end1};

        subrange<I1> result{end1, end1};
        I1 cursor = first1;
        while (cursor != end1) {
            auto match = search_fn{}(cursor, end1, first2, end2, pred, proj1,
                                     proj2);
            if (match.begin() == end1) break;
            result = match;
            cursor = match.begin();
            ++cursor;
        }
        return result;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<forward_range<R1> && forward_range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr auto operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto result = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              std::move(pred), std::move(proj1),
                              std::move(proj2));
        if constexpr (borrowed_range<R1>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr find_end_fn find_end{};

struct is_permutation_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Pred pred = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2 &&
               static_cast<bool>(std::invoke(
                   pred, std::invoke(proj1, *first1),
                   std::invoke(proj2, *first2)))) {
            ++first1;
            ++first2;
        }
        if (first1 == last1 || first2 == last2)
            return first1 == last1 && first2 == last2;

        for (I1 candidate = first1; candidate != last1; ++candidate) {
            bool counted = false;
            for (I1 previous = first1; previous != candidate; ++previous) {
                if (static_cast<bool>(std::invoke(
                        pred, std::invoke(proj1, *previous),
                        std::invoke(proj1, *candidate)))) {
                    counted = true;
                    break;
                }
            }
            if (counted) continue;

            iter_difference_t<I1> count1 = 0;
            for (I1 it = candidate; it != last1; ++it) {
                if (static_cast<bool>(std::invoke(
                        pred, std::invoke(proj1, *it),
                        std::invoke(proj1, *candidate)))) {
                    ++count1;
                }
            }
            iter_difference_t<I2> count2 = 0;
            for (I2 it = first2; it != last2; ++it) {
                if (static_cast<bool>(std::invoke(
                        pred, std::invoke(proj1, *candidate),
                        std::invoke(proj2, *it)))) {
                    ++count2;
                }
            }
            if (count1 != count2) return false;
        }
        return true;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<forward_range<R1> && forward_range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
    }
};

inline constexpr is_permutation_fn is_permutation{};

struct all_of_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Pred pred,
                              Proj proj = {}) const {
        for (; first != last; ++first) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                return false;
            }
        }
        return true;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Pred pred, Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range),
                       std::move(pred), std::move(proj));
    }
};

inline constexpr all_of_fn all_of{};

struct any_of_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Pred pred,
                              Proj proj = {}) const {
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                return true;
            }
        }
        return false;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Pred pred, Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range),
                       std::move(pred), std::move(proj));
    }
};

inline constexpr any_of_fn any_of{};

struct none_of_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Pred pred,
                              Proj proj = {}) const {
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                return false;
            }
        }
        return true;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<
                                 Pred, Proj, iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Pred pred, Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range),
                       std::move(pred), std::move(proj));
    }
};

inline constexpr none_of_fn none_of{};

template<typename I, typename F>
struct in_fun_result {
    I in;
    F fun;

    template<typename I2, typename F2>
    requires convertible_to<const I&, I2> && convertible_to<const F&, F2>
    constexpr operator in_fun_result<I2, F2>() const & {
        return {in, fun};
    }

    template<typename I2, typename F2>
    requires convertible_to<I, I2> && convertible_to<F, F2>
    constexpr operator in_fun_result<I2, F2>() && {
        return {std::move(in), std::move(fun)};
    }
};

struct for_each_fn {
    template<typename I, typename S, typename F, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             is_invocable<Proj&, iter_reference_t<I>>::value &&
                             is_invocable<F&, decltype(std::invoke(
                                 declval<Proj&>(), *declval<I&>()))>::value,
                         int> = 0>
    constexpr in_fun_result<I, F> operator()(I first, S last, F fun,
                                             Proj proj = {}) const {
        for (; first != last; ++first) {
            std::invoke(fun, std::invoke(proj, *first));
        }
        return {first, std::move(fun)};
    }

    template<typename R, typename F, typename Proj = identity,
             enable_if_t<range<R> &&
                             is_invocable<Proj&, iter_reference_t<
                                 iterator_t<R>>>::value &&
                             is_invocable<F&, decltype(std::invoke(
                                 declval<Proj&>(), *declval<
                                     iterator_t<R>&>()))>::value,
                         int> = 0>
    constexpr in_fun_result<borrowed_iterator_t<R>, F>
    operator()(R&& range, F fun, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(fun), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {result.in, std::move(result.fun)};
        } else {
            return {dangling(result.in), std::move(result.fun)};
        }
    }
};

inline constexpr for_each_fn for_each{};

struct for_each_n_fn {
    template<typename I, typename F, typename Proj = identity,
             enable_if_t<input_iterator<I> &&
                             is_invocable<Proj&, iter_reference_t<I>>::value &&
                             is_invocable<F&, decltype(std::invoke(
                                 declval<Proj&>(), *declval<I&>()))>::value,
                         int> = 0>
    constexpr in_fun_result<I, F> operator()(I first,
                                             iter_difference_t<I> count,
                                             F fun, Proj proj = {}) const {
        while (count > 0) {
            std::invoke(fun, std::invoke(proj, *first));
            ++first;
            --count;
        }
        return {first, std::move(fun)};
    }
};

inline constexpr for_each_n_fn for_each_n{};

template<typename I, typename O>
struct in_out_result {
    I in;
    O out;

    template<typename I2, typename O2>
    requires convertible_to<const I&, I2> && convertible_to<const O&, O2>
    constexpr operator in_out_result<I2, O2>() const & {
        return {in, out};
    }

    template<typename I2, typename O2>
    requires convertible_to<I, I2> && convertible_to<O, O2>
    constexpr operator in_out_result<I2, O2>() && {
        return {std::move(in), std::move(out)};
    }
};

namespace detail {

template<typename I, typename O, typename = void>
struct copy_assignment_valid : false_type {};

template<typename I, typename O>
struct copy_assignment_valid<I, O,
    void_t<decltype(*declval<O&>() = *declval<I&>())>> : true_type {};

template<typename I, typename O, typename = void>
struct move_assignment_valid : false_type {};

template<typename I, typename O>
struct move_assignment_valid<I, O,
    void_t<decltype(*declval<O&>() = declval<iter_value_t<I>&&>())>>
    : true_type {};

template<typename O, typename T, typename = void>
struct fill_assignment_valid : false_type {};

template<typename O, typename T>
struct fill_assignment_valid<O, T,
    void_t<decltype(*declval<O&>() = declval<const T&>())>> : true_type {};

template<typename F, typename = void>
struct generator_valid : false_type {};

template<typename F>
struct generator_valid<F, void_t<decltype(std::invoke(declval<F&>()))>>
    : true_type {};

template<typename I1, typename I2, typename = void>
struct swap_ranges_valid : false_type {};

template<typename I1, typename I2>
struct swap_ranges_valid<I1, I2,
    void_t<decltype(std::swap(*declval<I1&>(), *declval<I2&>()))>>
    : true_type {};

} /* namespace detail */

template<typename I1, typename I2>
struct in_in_result {
    I1 in1;
    I2 in2;

    template<typename J1, typename J2>
    requires convertible_to<const I1&, J1> && convertible_to<const I2&, J2>
    constexpr operator in_in_result<J1, J2>() const & {
        return {in1, in2};
    }

    template<typename J1, typename J2>
    requires convertible_to<I1, J1> && convertible_to<I2, J2>
    constexpr operator in_in_result<J1, J2>() && {
        return {std::move(in1), std::move(in2)};
    }
};

struct copy_fn {
    template<typename I, typename S, typename O,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result) const {
        while (first != last) {
            *result = *first;
            ++first;
            ++result;
        }
        return {first, result};
    }

    template<typename R, typename O,
             enable_if_t<range<R> && input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<
                                 iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result);
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr copy_fn copy{};

struct copy_n_fn {
    template<typename I, typename O,
             enable_if_t<input_iterator<I> && input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, iter_difference_t<I> count,
                                             O result) const {
        while (count > 0) {
            *result = *first;
            ++first;
            ++result;
            --count;
        }
        return {first, result};
    }
};

inline constexpr copy_n_fn copy_n{};

struct move_fn {
    template<typename I, typename S, typename O,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::move_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result) const {
        while (first != last) {
            *result = std::move(*first);
            ++first;
            ++result;
        }
        return {first, result};
    }

    template<typename R, typename O,
             enable_if_t<range<R> && input_or_output_iterator<O> &&
                             detail::move_assignment_valid<
                                 iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result) const {
        auto moved = (*this)(ranges::begin(range), ranges::end(range), result);
        if constexpr (borrowed_range<R>) {
            return {moved.in, moved.out};
        } else {
            return {dangling(moved.in), moved.out};
        }
    }
};

inline constexpr move_fn move{};

struct copy_if_fn {
    template<typename I, typename S, typename O, typename Pred,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             Pred pred, Proj proj = {}) const {
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                *result = *first;
                ++result;
            }
        }
        return {first, result};
    }

    template<typename R, typename O, typename Pred, typename Proj = identity,
             enable_if_t<range<R> && input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, Pred pred, Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr copy_if_fn copy_if{};

struct copy_backward_fn {
    template<typename I, typename S, typename O,
             enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                             bidirectional_iterator<O> &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result) const {
        I tail = first;
        while (tail != last) ++tail;
        while (tail != first) {
            --tail;
            --result;
            *result = *tail;
        }
        return {first, result};
    }

    template<typename R, typename O,
             enable_if_t<bidirectional_range<R> && bidirectional_iterator<O> &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result);
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr copy_backward_fn copy_backward{};

struct move_backward_fn {
    template<typename I, typename S, typename O,
             enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                             bidirectional_iterator<O> &&
                             detail::move_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result) const {
        I tail = first;
        while (tail != last) ++tail;
        while (tail != first) {
            --tail;
            --result;
            *result = std::move(*tail);
        }
        return {first, result};
    }

    template<typename R, typename O,
             enable_if_t<bidirectional_range<R> && bidirectional_iterator<O> &&
                             detail::move_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result) const {
        auto moved = (*this)(ranges::begin(range), ranges::end(range), result);
        if constexpr (borrowed_range<R>) {
            return {moved.in, moved.out};
        } else {
            return {dangling(moved.in), moved.out};
        }
    }
};

inline constexpr move_backward_fn move_backward{};

struct reverse_copy_fn {
    template<typename I, typename S, typename O,
             enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result) const {
        I tail = first;
        while (tail != last) ++tail;
        while (tail != first) {
            --tail;
            *result = *tail;
            ++result;
        }
        return {first, result};
    }

    template<typename R, typename O,
             enable_if_t<bidirectional_range<R> && input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result);
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr reverse_copy_fn reverse_copy{};

struct replace_copy_fn {
    template<typename I, typename S, typename O, typename T1, typename T2,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::find_projection_valid<Proj, I, T1>::value &&
                             detail::copy_assignment_valid<I, O>::value &&
                             detail::fill_assignment_valid<O, T2>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             const T1& old_value,
                                             const T2& new_value,
                                             Proj proj = {}) const {
        for (; first != last; ++first, ++result) {
            if (static_cast<bool>(std::invoke(proj, *first) == old_value))
                *result = new_value;
            else *result = *first;
        }
        return {first, result};
    }

    template<typename R, typename O, typename T1, typename T2,
             typename Proj = identity,
             enable_if_t<range<R> && input_or_output_iterator<O> &&
                             detail::find_projection_valid<Proj,
                                 iterator_t<R>, T1>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value &&
                             detail::fill_assignment_valid<O, T2>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, const T1& old_value,
               const T2& new_value, Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              old_value, new_value, std::move(proj));
        if constexpr (borrowed_range<R>) return {copied.in, copied.out};
        else return {dangling(copied.in), copied.out};
    }
};

inline constexpr replace_copy_fn replace_copy{};

struct replace_copy_if_fn {
    template<typename I, typename S, typename O, typename Pred, typename T,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::copy_assignment_valid<I, O>::value &&
                             detail::fill_assignment_valid<O, T>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             Pred pred,
                                             const T& new_value,
                                             Proj proj = {}) const {
        for (; first != last; ++first, ++result) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) *result = new_value;
            else *result = *first;
        }
        return {first, result};
    }

    template<typename R, typename O, typename Pred, typename T,
             typename Proj = identity,
             enable_if_t<range<R> && input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value &&
                             detail::fill_assignment_valid<O, T>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, Pred pred, const T& new_value,
               Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              std::move(pred), new_value, std::move(proj));
        if constexpr (borrowed_range<R>) return {copied.in, copied.out};
        else return {dangling(copied.in), copied.out};
    }
};

inline constexpr replace_copy_if_fn replace_copy_if{};

struct mismatch_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr in_in_result<I1, I2> operator()(I1 first1, S1 last1,
                                              I2 first2, S2 last2,
                                              Pred pred = {},
                                              Proj1 proj1 = {},
                                              Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2 &&
               static_cast<bool>(std::invoke(
                   pred, std::invoke(proj1, *first1),
                   std::invoke(proj2, *first2)))) {
            ++first1;
            ++first2;
        }
        return {first1, first2};
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<range<R1> && range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr in_in_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>>
    operator()(R1&& range1, R2&& range2, Pred pred = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto result = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              std::move(pred), std::move(proj1),
                              std::move(proj2));
        if constexpr (borrowed_range<R1> && borrowed_range<R2>) return result;
        else return {borrowed_range<R1> ? result.in1 : dangling(result.in1),
                     borrowed_range<R2> ? result.in2 : dangling(result.in2)};
    }
};

inline constexpr mismatch_fn mismatch{};

struct lexicographical_compare_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Comp comp = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) return true;
            if (static_cast<bool>(std::invoke(comp, right, left))) return false;
            ++first1;
            ++first2;
        }
        return first1 == last1 && first2 != last2;
    }

    template<typename R1, typename R2, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<range<R1> && range<R2> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Comp comp = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(comp), std::move(proj1), std::move(proj2));
    }
};

inline constexpr lexicographical_compare_fn lexicographical_compare{};

namespace detail {

template<typename Comp, typename Proj1, typename Proj2, typename I1,
         typename I2, typename = void>
struct lexicographical_three_way_valid : false_type {};

template<typename Comp, typename Proj1, typename Proj2, typename I1,
         typename I2>
struct lexicographical_three_way_valid<
    Comp, Proj1, Proj2, I1, I2,
    void_t<decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj1&>(), *declval<I1&>()),
               std::invoke(declval<Proj2&>(), *declval<I2&>()))),
           decltype(remove_cvref_t<decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj1&>(), *declval<I1&>()),
               std::invoke(declval<Proj2&>(), *declval<I2&>())))>::less),
           decltype(remove_cvref_t<decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj1&>(), *declval<I1&>()),
               std::invoke(declval<Proj2&>(), *declval<I2&>())))>::greater),
           decltype(remove_cvref_t<decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj1&>(), *declval<I1&>()),
               std::invoke(declval<Proj2&>(), *declval<I2&>())))>::equivalent)> >
    : true_type {};

} /* namespace detail */

struct lexicographical_compare_three_way_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Comp = compare_three_way, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::lexicographical_three_way_valid<
                                 Comp, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr auto operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Comp comp = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const
        -> decltype(std::invoke(comp,
                                std::invoke(proj1, *first1),
                                std::invoke(proj2, *first2))) {
        using result_type = remove_cvref_t<decltype(std::invoke(
            comp, std::invoke(proj1, *first1),
            std::invoke(proj2, *first2)))>;
        while (first1 != last1 && first2 != last2) {
            auto result = std::invoke(comp, std::invoke(proj1, *first1),
                                      std::invoke(proj2, *first2));
            if (result != 0) return result;
            ++first1;
            ++first2;
        }
        if (first1 == last1 && first2 == last2) return result_type::equivalent;
        return first1 == last1 ? result_type::less : result_type::greater;
    }

    template<typename R1, typename R2, typename Comp = compare_three_way,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             detail::lexicographical_three_way_valid<
                                 Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr auto operator()(R1&& range1, R2&& range2, Comp comp = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const
        -> decltype((*this)(ranges::begin(range1), ranges::end(range1),
                            ranges::begin(range2), ranges::end(range2),
                            std::move(comp), std::move(proj1),
                            std::move(proj2))) {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(comp), std::move(proj1), std::move(proj2));
    }
};

inline constexpr lexicographical_compare_three_way_fn
    lexicographical_compare_three_way{};

struct unique_fn {
    template<typename I, typename S, typename Comp = equal_to<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::move_assignment_valid<I, I>::value,
                         int> = 0>
    constexpr subrange<I, S> operator()(I first, S last, Comp comp = {},
                                        Proj proj = {}) const {
        if (first == last) return {first, last};
        I result = first;
        I cursor = first;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (!static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *result),
                    std::invoke(proj, *cursor)))) {
                ++result;
                *result = std::move(*cursor);
            }
        }
        ++result;
        return {result, last};
    }

    template<typename R, typename Comp = equal_to<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::move_assignment_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, Comp comp = {}, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr unique_fn unique{};

struct unique_copy_fn {
    template<typename I, typename S, typename O, typename Comp = equal_to<>,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             Comp comp = {},
                                             Proj proj = {}) const {
        if (first == last) return {first, result};
        iter_value_t<I> previous = *first;
        *result = previous;
        ++result;
        ++first;
        for (; first != last; ++first) {
            if (!static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, previous),
                    std::invoke(proj, *first)))) {
                previous = *first;
                *result = *first;
                ++result;
            }
        }
        return {first, result};
    }

    template<typename R, typename O, typename Comp = equal_to<>,
             typename Proj = identity,
             enable_if_t<input_range<R> && input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, Comp comp = {}, Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return {copied.in, copied.out};
        else return {dangling(copied.in), copied.out};
    }
};

inline constexpr unique_copy_fn unique_copy{};

#if __cplusplus > 202002L
struct contains_fn {
    template<typename R, typename T, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::find_projection_valid<
                                 Proj, iterator_t<R>, T>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, const T& value,
                              Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        return find_fn{}(first, last, value, std::move(proj)) != last;
    }
};

inline constexpr contains_fn contains{};
#endif

/* ???????????????????????????????????????????????????????????????
 * fill / generate range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct fill_fn {
    template<typename I, typename S, typename T,
             enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I> &&
                             detail::fill_assignment_valid<I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, const T& value) const {
        for (; first != last; ++first) *first = value;
        return first;
    }

    template<typename R, typename T,
             enable_if_t<range<R> &&
                             detail::fill_assignment_valid<iterator_t<R>, T>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, const T& value) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range), value);
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr fill_fn fill{};

struct fill_n_fn {
    template<typename I, typename T,
             enable_if_t<input_or_output_iterator<I> &&
                             detail::fill_assignment_valid<I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, iter_difference_t<I> count,
                           const T& value) const {
        while (count > 0) {
            *first = value;
            ++first;
            --count;
        }
        return first;
    }
};

inline constexpr fill_n_fn fill_n{};

struct generate_fn {
    template<typename I, typename S, typename F,
             enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I> &&
                             detail::generator_valid<F>::value &&
                             detail::fill_assignment_valid<I,
                                 remove_cvref_t<invoke_result_t<F&>>>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, F gen) const {
        for (; first != last; ++first) *first = std::invoke(gen);
        return first;
    }

    template<typename R, typename F,
             enable_if_t<range<R> && detail::generator_valid<F>::value &&
                             detail::fill_assignment_valid<iterator_t<R>,
                                 remove_cvref_t<invoke_result_t<F&>>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, F gen) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(gen));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr generate_fn generate{};

struct generate_n_fn {
    template<typename I, typename F,
             enable_if_t<input_or_output_iterator<I> &&
                             detail::generator_valid<F>::value &&
                             detail::fill_assignment_valid<I,
                                 remove_cvref_t<invoke_result_t<F&>>>::value,
                         int> = 0>
    constexpr I operator()(I first, iter_difference_t<I> count, F gen) const {
        while (count > 0) {
            *first = std::invoke(gen);
            ++first;
            --count;
        }
        return first;
    }
};

inline constexpr generate_n_fn generate_n{};

/* ???????????????????????????????????????????????????????????????
 * replace / remove / swap range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct replace_fn {
    template<typename I, typename S, typename T1, typename T2,
             typename Proj = identity,
             enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I> &&
                             detail::find_projection_valid<Proj, I, T1>::value &&
                             detail::fill_assignment_valid<I, T2>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, const T1& old_value,
                           const T2& new_value, Proj proj = {}) const {
        for (; first != last; ++first)
            if (static_cast<bool>(std::invoke(proj, *first) == old_value))
                *first = new_value;
        return first;
    }

    template<typename R, typename T1, typename T2, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::find_projection_valid<Proj,
                                 iterator_t<R>, T1>::value &&
                             detail::fill_assignment_valid<iterator_t<R>, T2>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range,
                                                 const T1& old_value,
                                                  const T2& new_value,
                                                  Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              old_value, new_value, std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr replace_fn replace{};

struct replace_if_fn {
    template<typename I, typename S, typename Pred, typename T,
             typename Proj = identity,
             enable_if_t<input_or_output_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::fill_assignment_valid<I, T>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           const T& new_value, Proj proj = {}) const {
        for (; first != last; ++first)
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first))))
                *first = new_value;
        return first;
    }

    template<typename R, typename Pred, typename T, typename Proj = identity,
             enable_if_t<range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::fill_assignment_valid<iterator_t<R>, T>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                  const T& new_value,
                                                  Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), new_value, std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr replace_if_fn replace_if{};

struct remove_if_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::move_assignment_valid<I, I>::value,
                         int> = 0>
    constexpr subrange<I, S> operator()(I first, S last, Pred pred,
                                        Proj proj = {}) const {
        first = find_if_fn{}(first, last, pred, proj);
        if (first == last) return {first, last};
        auto cursor = first;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *cursor)))) {
                *first = std::move(*cursor);
                ++first;
            }
        }
        return {first, last};
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::move_assignment_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, Pred pred, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr remove_if_fn remove_if{};

struct remove_fn {
    template<typename I, typename S, typename T, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::find_projection_valid<Proj, I, T>::value &&
                             detail::move_assignment_valid<I, I>::value,
                         int> = 0>
    constexpr subrange<I, S> operator()(I first, S last,
                                        const T& value, Proj proj = {}) const {
        return remove_if_fn{}(first, last,
            [&](const auto& element) { return std::invoke(proj, element) == value; });
    }

    template<typename R, typename T, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::find_projection_valid<Proj,
                                 iterator_t<R>, T>::value &&
                             detail::move_assignment_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, const T& value, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range), value,
                              std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr remove_fn remove{};

struct remove_copy_if_fn {
    template<typename I, typename S, typename O, typename Pred,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             Pred pred, Proj proj = {}) const {
        for (; first != last; ++first) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                *result = *first;
                ++result;
            }
        }
        return {first, result};
    }

    template<typename R, typename O, typename Pred, typename Proj = identity,
             enable_if_t<input_range<R> && input_or_output_iterator<O> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, Pred pred, Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr remove_copy_if_fn remove_copy_if{};

struct remove_copy_fn {
    template<typename I, typename S, typename O, typename T,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::find_projection_valid<Proj, I, T>::value &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result,
                                             const T& value,
                                             Proj proj = {}) const {
        return remove_copy_if_fn{}(
            first, last, result,
            [&](const auto& element) {
                return static_cast<bool>(std::invoke(proj, element) == value);
            });
    }

    template<typename R, typename O, typename T, typename Proj = identity,
             enable_if_t<input_range<R> && input_or_output_iterator<O> &&
                             detail::find_projection_valid<Proj,
                                 iterator_t<R>, T>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, const T& value, Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), result,
                              value, std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out};
        } else {
            return {dangling(copied.in), copied.out};
        }
    }
};

inline constexpr remove_copy_fn remove_copy{};

struct swap_ranges_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::swap_ranges_valid<I1, I2>::value,
                         int> = 0>
    constexpr in_in_result<I1, I2> operator()(I1 first1, S1 last1,
                                              I2 first2, S2 last2) const {
        while (first1 != last1 && first2 != last2) {
            std::swap(*first1, *first2);
            ++first1;
            ++first2;
        }
        return {first1, first2};
    }

    template<typename R1, typename R2,
             enable_if_t<forward_range<R1> && forward_range<R2> &&
                             detail::swap_ranges_valid<iterator_t<R1>,
                                 iterator_t<R2>>::value,
                         int> = 0>
    constexpr in_in_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>>
    operator()(R1&& range1, R2&& range2) const {
        auto result = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2));
        if constexpr (borrowed_range<R1> && borrowed_range<R2>) {
            return result;
        } else {
            return {borrowed_range<R1> ? result.in1 : dangling(result.in1),
                    borrowed_range<R2> ? result.in2 : dangling(result.in2)};
        }
    }
};

inline constexpr swap_ranges_fn swap_ranges{};

struct rotate_fn {
private:
    template<typename I>
    static constexpr I rotate_impl(I first, I middle, I last) {
        if (first == middle) return last;
        if (middle == last) return first;
        I read = middle;
        I write = first;
        I next_read = first;
        while (read != last) {
            if (write == next_read) next_read = read;
            std::swap(*write, *read);
            ++write;
            ++read;
        }
        return rotate_impl(write, next_read, last);
    }

public:
    template<typename I, typename S,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I>, int> = 0>
    constexpr subrange<I, S> operator()(I first, I middle, S last) const {
        I tail = first;
        while (tail != last) ++tail;
        I new_middle = first;
        for (I cursor = middle; cursor != tail; ++cursor) ++new_middle;
        rotate_impl(first, middle, tail);
        return {new_middle, last};
    }

    template<typename R, enable_if_t<forward_range<R>, int> = 0>
    constexpr auto operator()(R&& range, iterator_t<R> middle) const {
        auto result = (*this)(ranges::begin(range), middle, ranges::end(range));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr rotate_fn rotate{};

struct rotate_copy_fn {
    template<typename I, typename S, typename O,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, I middle, S last,
                                             O result) const {
        I tail = first;
        while (tail != last) ++tail;
        for (I cursor = middle; cursor != tail; ++cursor, ++result)
            *result = *cursor;
        for (I cursor = first; cursor != middle; ++cursor, ++result)
            *result = *cursor;
        return {tail, result};
    }

    template<typename R, typename O,
             enable_if_t<forward_range<R> && input_or_output_iterator<O> &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, iterator_t<R> middle, O result) const {
        auto copied = (*this)(ranges::begin(range), middle, ranges::end(range),
                              result);
        if constexpr (borrowed_range<R>) return {copied.in, copied.out};
        else return {dangling(copied.in), copied.out};
    }
};

inline constexpr rotate_copy_fn rotate_copy{};

template<typename I, typename O1, typename O2>
struct in_out_out_result {
    I in;
    O1 out1;
    O2 out2;

    template<typename I2, typename O12, typename O22>
    requires convertible_to<const I&, I2> &&
        convertible_to<const O1&, O12> && convertible_to<const O2&, O22>
    constexpr operator in_out_out_result<I2, O12, O22>() const & {
        return {in, out1, out2};
    }

    template<typename I2, typename O12, typename O22>
    requires convertible_to<I, I2> && convertible_to<O1, O12> &&
        convertible_to<O2, O22>
    constexpr operator in_out_out_result<I2, O12, O22>() && {
        return {std::move(in), std::move(out1), std::move(out2)};
    }
};

struct partition_copy_fn {
    template<typename I, typename S, typename O1, typename O2,
             typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O1> &&
                             input_or_output_iterator<O2> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::copy_assignment_valid<I, O1>::value &&
                             detail::copy_assignment_valid<I, O2>::value,
                         int> = 0>
    constexpr in_out_out_result<I, O1, O2>
    operator()(I first, S last, O1 out_true, O2 out_false, Pred pred,
               Proj proj = {}) const {
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) {
                *out_true = *first;
                ++out_true;
            } else {
                *out_false = *first;
                ++out_false;
            }
        }
        return {first, out_true, out_false};
    }

    template<typename R, typename O1, typename O2, typename Pred,
             typename Proj = identity,
             enable_if_t<input_range<R> && input_or_output_iterator<O1> &&
                             input_or_output_iterator<O2> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O1>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O2>::value,
                         int> = 0>
    constexpr in_out_out_result<borrowed_iterator_t<R>, O1, O2>
    operator()(R&& range, O1 out_true, O2 out_false, Pred pred,
               Proj proj = {}) const {
        auto copied = (*this)(ranges::begin(range), ranges::end(range), out_true,
                              out_false, std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {copied.in, copied.out1, copied.out2};
        } else {
            return {dangling(copied.in), copied.out1, copied.out2};
        }
    }
};

inline constexpr partition_copy_fn partition_copy{};

struct is_partitioned_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Pred pred,
                              Proj proj = {}) const {
        while (first != last && static_cast<bool>(std::invoke(
                   pred, std::invoke(proj, *first)))) ++first;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *first)))) return false;
        }
        return true;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<input_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Pred pred, Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range),
                       std::move(pred), std::move(proj));
    }
};

inline constexpr is_partitioned_fn is_partitioned{};

struct partition_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr subrange<I, S> operator()(I first, S last, Pred pred,
                                        Proj proj = {}) const {
        I middle = first;
        while (middle != last && static_cast<bool>(std::invoke(
                   pred, std::invoke(proj, *middle)))) ++middle;
        if (middle == last) return {middle, last};
        I cursor = middle;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, *cursor)))) {
                std::swap(*middle, *cursor);
                ++middle;
            }
        }
        return {middle, last};
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, Pred pred, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr partition_fn partition{};

struct stable_partition_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr subrange<I, S> operator()(I first, S last, Pred pred,
                                        Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        I middle = std::stable_partition(
            first, end,
            [&](const auto& value) {
                return static_cast<bool>(std::invoke(
                    pred, std::invoke(proj, value)));
            });
        return {middle, last};
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<bidirectional_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, Pred pred, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result.begin());
    }
};

inline constexpr stable_partition_fn stable_partition{};

struct partition_point_fn {
    template<typename I, typename S, typename Pred, typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::unary_predicate_valid<Pred, Proj, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Pred pred,
                           Proj proj = {}) const {
        while (first != last && static_cast<bool>(std::invoke(
                   pred, std::invoke(proj, *first)))) ++first;
        return first;
    }

    template<typename R, typename Pred, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::unary_predicate_valid<Pred, Proj,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Pred pred,
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(pred), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr partition_point_fn partition_point{};

template<typename I1, typename I2, typename O>
struct in_in_out_result {
    I1 in1;
    I2 in2;
    O out;

    template<typename J1, typename J2, typename O2>
    requires convertible_to<const I1&, J1> && convertible_to<const I2&, J2> &&
        convertible_to<const O&, O2>
    constexpr operator in_in_out_result<J1, J2, O2>() const & {
        return {in1, in2, out};
    }

    template<typename J1, typename J2, typename O2>
    requires convertible_to<I1, J1> && convertible_to<I2, J2> &&
        convertible_to<O, O2>
    constexpr operator in_in_out_result<J1, J2, O2>() && {
        return {std::move(in1), std::move(in2), std::move(out)};
    }
};

template<typename R1, typename R2, typename I1, typename I2, typename O>
constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
make_in_in_out_result(in_in_out_result<I1, I2, O> result) {
    if constexpr (borrowed_range<R1> && borrowed_range<R2>) {
        return {result.in1, result.in2, result.out};
    } else if constexpr (borrowed_range<R1>) {
        return {result.in1, dangling(result.in2), result.out};
    } else if constexpr (borrowed_range<R2>) {
        return {dangling(result.in1), result.in2, result.out};
    } else {
        return {dangling(result.in1), dangling(result.in2), result.out};
    }
}

struct inplace_merge_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<bidirectional_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, I middle, S last, Comp comp = {},
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        std::inplace_merge(
            first, middle, end,
            [&](const auto& left, const auto& right) {
                return static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, left),
                    std::invoke(proj, right)));
            });
        return end;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<bidirectional_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range,
                                                 iterator_t<R> middle,
                                                 Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), middle, ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr inplace_merge_fn inplace_merge{};

namespace detail {

template<typename G, typename = void>
struct uniform_random_valid : false_type {};

template<typename G>
struct uniform_random_valid<
    G, void_t<typename remove_reference<G>::type::result_type,
              decltype(remove_reference_t<G>::min()),
              decltype(remove_reference_t<G>::max()),
              decltype(declval<G&>()())>>
    : integral_constant<bool,
          is_integral<typename remove_reference<G>::type::result_type>::value &&
          is_unsigned<typename remove_reference<G>::type::result_type>::value &&
          is_convertible<decltype(remove_reference_t<G>::min()),
                         typename remove_reference<G>::type::result_type>::value &&
          is_convertible<decltype(remove_reference_t<G>::max()),
                         typename remove_reference<G>::type::result_type>::value> {};

} /* namespace detail */

struct shuffle_fn {
    template<typename I, typename S, typename G,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::uniform_random_valid<G>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, G&& generator) const {
        I end = first;
        while (end != last) ++end;
        std::shuffle(first, end, std::forward<G>(generator));
        return end;
    }

    template<typename R, typename G,
             enable_if_t<random_access_range<R> &&
                             detail::uniform_random_valid<G>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range,
                                                 G&& generator) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::forward<G>(generator));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr shuffle_fn shuffle{};

struct sample_fn {
    template<typename I, typename S, typename O, typename D, typename G,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::uniform_random_valid<G>::value &&
                             detail::copy_assignment_valid<I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O output,
                                             D count, G&& generator) const {
        I end = first;
        while (end != last) ++end;
        O result = std::sample(first, end, output, count,
                               std::forward<G>(generator));
        return {end, result};
    }

    template<typename R, typename O, typename D, typename G,
             enable_if_t<input_range<R> && input_or_output_iterator<O> &&
                             detail::uniform_random_valid<G>::value &&
                             detail::copy_assignment_valid<iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O output, D count, G&& generator) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range), output,
                              count, std::forward<G>(generator));
        if constexpr (borrowed_range<R>) {
            return {result.in, result.out};
        } else {
            return {dangling(result.in), result.out};
        }
    }
};

inline constexpr sample_fn sample{};

struct merge_fn {
    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj1, Proj2, I1, I2>::value &&
                             detail::copy_assignment_valid<I1, O>::value &&
                             detail::copy_assignment_valid<I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = {}, Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj2, *first2),
                    std::invoke(proj1, *first1)))) {
                *result = *first2;
                ++first2;
            } else {
                *result = *first1;
                ++first1;
            }
            ++result;
        }
        while (first1 != last1) {
            *result = *first1;
            ++first1;
            ++result;
        }
        while (first2 != last2) {
            *result = *first2;
            ++first2;
            ++result;
        }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value &&
                             detail::copy_assignment_valid<iterator_t<R1>, O>::value &&
                             detail::copy_assignment_valid<iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, Comp comp = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto merged = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              result, std::move(comp), std::move(proj1),
                              std::move(proj2));
        return make_in_in_out_result<R1, R2>(merged);
    }
};

inline constexpr merge_fn merge{};

struct set_union_fn {
    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj1, Proj2, I1, I2>::value &&
                             detail::copy_assignment_valid<I1, O>::value &&
                             detail::copy_assignment_valid<I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = {}, Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) {
                *result = *first1;
                ++first1;
            } else if (static_cast<bool>(std::invoke(comp, right, left))) {
                *result = *first2;
                ++first2;
            } else {
                *result = *first1;
                ++first1;
                ++first2;
            }
            ++result;
        }
        while (first1 != last1) { *result = *first1; ++first1; ++result; }
        while (first2 != last2) { *result = *first2; ++first2; ++result; }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value &&
                             detail::copy_assignment_valid<iterator_t<R1>, O>::value &&
                             detail::copy_assignment_valid<iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, Comp comp = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto merged = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              result, std::move(comp), std::move(proj1),
                              std::move(proj2));
        return make_in_in_out_result<R1, R2>(merged);
    }
};

inline constexpr set_union_fn set_union{};

struct set_intersection_fn {
    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 I1, I2>::value &&
                             detail::copy_assignment_valid<I1, O>::value &&
                             detail::copy_assignment_valid<I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = {}, Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) ++first1;
            else if (static_cast<bool>(std::invoke(comp, right, left))) ++first2;
            else { *result = *first1; ++result; ++first1; ++first2; }
        }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value &&
                             detail::copy_assignment_valid<iterator_t<R1>, O>::value &&
                             detail::copy_assignment_valid<iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, Comp comp = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto merged = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2), result,
                              std::move(comp), std::move(proj1), std::move(proj2));
        return make_in_in_out_result<R1, R2>(merged);
    }
};

inline constexpr set_intersection_fn set_intersection{};

struct set_difference_fn {
    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 I1, I2>::value &&
                             detail::copy_assignment_valid<I1, O>::value &&
                             detail::copy_assignment_valid<I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = {}, Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) {
                *result = *first1; ++result; ++first1;
            } else if (static_cast<bool>(std::invoke(comp, right, left))) ++first2;
            else { ++first1; ++first2; }
        }
        while (first1 != last1) { *result = *first1; ++first1; ++result; }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value &&
                             detail::copy_assignment_valid<iterator_t<R1>, O>::value &&
                             detail::copy_assignment_valid<iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, Comp comp = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto merged = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2), result,
                              std::move(comp), std::move(proj1), std::move(proj2));
        return make_in_in_out_result<R1, R2>(merged);
    }
};

inline constexpr set_difference_fn set_difference{};

struct set_symmetric_difference_fn {
    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 I1, I2>::value &&
                             detail::copy_assignment_valid<I1, O>::value &&
                             detail::copy_assignment_valid<I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result,
               Comp comp = {}, Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) {
                *result = *first1; ++result; ++first1;
            } else if (static_cast<bool>(std::invoke(comp, right, left))) {
                *result = *first2; ++result; ++first2;
            } else { ++first1; ++first2; }
        }
        while (first1 != last1) { *result = *first1; ++first1; ++result; }
        while (first2 != last2) { *result = *first2; ++first2; ++result; }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value &&
                             detail::copy_assignment_valid<iterator_t<R1>, O>::value &&
                             detail::copy_assignment_valid<iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, Comp comp = {},
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto merged = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2), result,
                              std::move(comp), std::move(proj1), std::move(proj2));
        return make_in_in_out_result<R1, R2>(merged);
    }
};

inline constexpr set_symmetric_difference_fn set_symmetric_difference{};

struct includes_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Comp = less<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Comp comp = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        while (first2 != last2) {
            if (first1 == last1) return false;
            auto&& left = std::invoke(proj1, *first1);
            auto&& right = std::invoke(proj2, *first2);
            if (static_cast<bool>(std::invoke(comp, left, right))) {
                ++first1;
            } else if (static_cast<bool>(std::invoke(comp, right, left))) {
                return false;
            } else {
                ++first1;
                ++first2;
            }
        }
        return true;
    }

    template<typename R1, typename R2, typename Comp = less<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             detail::binary_predicate_valid<Comp, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Comp comp = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(comp), std::move(proj1), std::move(proj2));
    }
};

inline constexpr includes_fn includes{};

struct make_heap_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        I tail = first;
        while (tail != last) ++tail;
        std::make_heap(first, tail, [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(comp, std::invoke(proj, left),
                                                 std::invoke(proj, right)));
        });
        return tail;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr make_heap_fn make_heap{};

struct push_heap_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        I tail = first;
        while (tail != last) ++tail;
        std::push_heap(first, tail, [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(comp, std::invoke(proj, left),
                                                 std::invoke(proj, right)));
        });
        return tail;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr push_heap_fn push_heap{};

struct pop_heap_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        I tail = first;
        while (tail != last) ++tail;
        std::pop_heap(first, tail, [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(comp, std::invoke(proj, left),
                                                 std::invoke(proj, right)));
        });
        return tail;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr pop_heap_fn pop_heap{};

struct sort_heap_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        I tail = first;
        while (tail != last) ++tail;
        std::sort_heap(first, tail, [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(comp, std::invoke(proj, left),
                                                 std::invoke(proj, right)));
        });
        return tail;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr sort_heap_fn sort_heap{};

struct is_heap_until_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        I tail = first;
        while (tail != last) ++tail;
        auto length = tail - first;
        for (decltype(length) index = 1; index < length; ++index) {
            I parent = first + (index - 1) / 2;
            I child = first + index;
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *parent),
                    std::invoke(proj, *child)))) {
                return child;
            }
        }
        return tail;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr is_heap_until_fn is_heap_until{};

struct is_heap_fn {
    template<typename I, typename S, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Comp comp = {}, Proj proj = {}) const {
        return is_heap_until_fn{}(first, last, std::move(comp), std::move(proj)) == last;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Comp comp = {}, Proj proj = {}) const {
        return is_heap_until_fn{}(ranges::begin(range), ranges::end(range),
                                  std::move(comp), std::move(proj)) ==
               ranges::end(range);
    }
};

inline constexpr is_heap_fn is_heap{};

namespace detail {

template<typename Comp, typename Proj, typename I, typename T,
         typename = void>
struct value_compare_valid : false_type {};

template<typename Comp, typename Proj, typename I, typename T>
struct value_compare_valid<
    Comp, Proj, I, T,
    void_t<decltype(std::invoke(
                      declval<Comp&>(),
                      std::invoke(declval<Proj&>(), *declval<I&>()),
                      declval<const T&>())),
           decltype(std::invoke(
                      declval<Comp&>(), declval<const T&>(),
                      std::invoke(declval<Proj&>(), *declval<I&>())))>>
    : integral_constant<bool,
          is_convertible<decltype(std::invoke(
              declval<Comp&>(),
              std::invoke(declval<Proj&>(), *declval<I&>()),
              declval<const T&>())), bool>::value &&
          is_convertible<decltype(std::invoke(
              declval<Comp&>(), declval<const T&>(),
              std::invoke(declval<Proj&>(), *declval<I&>()))), bool>::value> {};

} /* namespace detail */

struct is_sorted_until_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {},
                           Proj proj = {}) const {
        if (first == last) return first;
        I next = first;
        ++next;
        for (; next != last; ++first, ++next) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *next),
                    std::invoke(proj, *first)))) {
                return next;
            }
        }
        return next;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr is_sorted_until_fn is_sorted_until{};

struct is_sorted_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, Comp comp = {},
                              Proj proj = {}) const {
        return is_sorted_until_fn{}(first, last, std::move(comp),
                                    std::move(proj)) == last;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, Comp comp = {}, Proj proj = {}) const {
        return is_sorted_until_fn{}(ranges::begin(range), ranges::end(range),
                                    std::move(comp), std::move(proj)) ==
               ranges::end(range);
    }
};

inline constexpr is_sorted_fn is_sorted{};

struct binary_search_fn {
    template<typename I, typename S, typename T, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::value_compare_valid<Comp, Proj, I, T>::value,
                         int> = 0>
    constexpr bool operator()(I first, S last, const T& value,
                              Comp comp = {}, Proj proj = {}) const {
        for (; first != last; ++first) {
            auto&& element = std::invoke(proj, *first);
            if (static_cast<bool>(std::invoke(comp, element, value))) continue;
            if (static_cast<bool>(std::invoke(comp, value, element))) return false;
            return true;
        }
        return false;
    }

    template<typename R, typename T, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::value_compare_valid<Comp, Proj,
                                 iterator_t<R>, T>::value,
                         int> = 0>
    constexpr bool operator()(R&& range, const T& value, Comp comp = {},
                              Proj proj = {}) const {
        return (*this)(ranges::begin(range), ranges::end(range), value,
                       std::move(comp), std::move(proj));
    }
};

inline constexpr binary_search_fn binary_search{};

#if __cplusplus > 202002L
namespace detail {

template<typename Pred, typename Proj1, typename Proj2, typename I1,
         typename I2, typename = void>
struct starts_ends_predicate_valid : false_type {};

template<typename Pred, typename Proj1, typename Proj2, typename I1,
         typename I2>
struct starts_ends_predicate_valid<
    Pred, Proj1, Proj2, I1, I2,
    void_t<decltype(std::invoke(
        declval<Pred&>(),
        std::invoke(declval<Proj1&>(), *declval<I1&>()),
        std::invoke(declval<Proj2&>(), *declval<I2&>())))>>
    : is_convertible<decltype(std::invoke(
          declval<Pred&>(),
          std::invoke(declval<Proj1&>(), *declval<I1&>()),
          std::invoke(declval<Proj2&>(), *declval<I2&>()))), bool> {};

} /* namespace detail */

/* ???????????????????????????????????????????????????????????????
 * starts_with / ends_with range algorithms
 * ???????????????????????????????????????????????????????????????*/

struct starts_with_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::starts_ends_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Pred pred = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        while (first2 != last2) {
            if (first1 == last1 || !static_cast<bool>(std::invoke(
                    pred, std::invoke(proj1, *first1),
                    std::invoke(proj2, *first2)))) {
                return false;
            }
            ++first1;
            ++first2;
        }
        return true;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<range<R1> && range<R2> &&
                             detail::starts_ends_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
    }
};

inline constexpr starts_with_fn starts_with{};

struct ends_with_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<forward_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::starts_ends_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr bool operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                              Pred pred = {}, Proj1 proj1 = {},
                              Proj2 proj2 = {}) const {
        ptrdiff_t first_size = 0;
        ptrdiff_t second_size = 0;
        for (I1 cursor = first1; cursor != last1; ++cursor) ++first_size;
        for (I2 cursor = first2; cursor != last2; ++cursor) ++second_size;
        if (second_size > first_size) return false;
        for (ptrdiff_t skip = first_size - second_size; skip != 0; --skip)
            ++first1;
        while (first2 != last2) {
            if (!static_cast<bool>(std::invoke(
                    pred, std::invoke(proj1, *first1),
                    std::invoke(proj2, *first2)))) {
                return false;
            }
            ++first1;
            ++first2;
        }
        return true;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<range<R1> && range<R2> &&
                             detail::starts_ends_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr bool operator()(R1&& range1, R2&& range2, Pred pred = {},
                              Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        return (*this)(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
    }
};

inline constexpr ends_with_fn ends_with{};
#endif

/* ???????????????????????????????????????????????????????????????
 * lower_bound - ??? (??????????)
 * ???????????????????????????????????????????????????????????????*/

template<typename I, typename S, typename T, typename Comp = less<>,
         typename Proj = identity,
         enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                         detail::value_compare_valid<Comp, Proj, I, T>::value,
                     int> = 0>
constexpr I lower_bound(I first, S last, const T& value, Comp comp = {},
                        Proj proj = {}) {
    iter_difference_t<I> length = 0;
    for (I cursor = first; cursor != last; ++cursor) ++length;
    while (length > 0) {
        iter_difference_t<I> half = length / 2;
        I middle = first;
        for (iter_difference_t<I> i = 0; i < half; ++i) ++middle;
        if (static_cast<bool>(std::invoke(
                comp, std::invoke(proj, *middle), value))) {
            first = middle;
            ++first;
            length -= half + 1;
        } else {
            length = half;
        }
    }
    return first;
}

template<typename R, typename T, typename Comp = less<>, typename Proj = identity,
         enable_if_t<range<R> && detail::value_compare_valid<Comp, Proj,
                             iterator_t<R>, T>::value, int> = 0>
constexpr auto lower_bound(R&& range, const T& value, Comp comp = {}, Proj proj = {}) {
    auto first = ranges::begin(range);
    auto last = ranges::end(range);

    /* Calculate size by iterating */
    ptrdiff_t len = 0;
    for (auto it = first; it != last; ++it) {
        ++len;
    }

    while (len > 0) {
        ptrdiff_t half = len / 2;
        auto mid = first;
        for (ptrdiff_t i = 0; i < half; ++i) ++mid;

        if (comp(std::invoke(proj, *mid), value)) {
            first = mid;
            ++first;
            len = len - half - 1;
        } else {
            len = half;
        }
    }

    return first;
}

/* ???????????????????????????????????????????????????????????????
 * upper_bound - ??? (??????????)
 * ???????????????????????????????????????????????????????????????*/

template<typename I, typename S, typename T, typename Comp = less<>,
         typename Proj = identity,
         enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                         detail::value_compare_valid<Comp, Proj, I, T>::value,
                     int> = 0>
constexpr I upper_bound(I first, S last, const T& value, Comp comp = {},
                        Proj proj = {}) {
    iter_difference_t<I> length = 0;
    for (I cursor = first; cursor != last; ++cursor) ++length;
    while (length > 0) {
        iter_difference_t<I> half = length / 2;
        I middle = first;
        for (iter_difference_t<I> i = 0; i < half; ++i) ++middle;
        if (!static_cast<bool>(std::invoke(
                comp, value, std::invoke(proj, *middle)))) {
            first = middle;
            ++first;
            length -= half + 1;
        } else {
            length = half;
        }
    }
    return first;
}

template<typename R, typename T, typename Comp = less<>, typename Proj = identity,
         enable_if_t<range<R> && detail::value_compare_valid<Comp, Proj,
                             iterator_t<R>, T>::value, int> = 0>
constexpr auto upper_bound(R&& range, const T& value, Comp comp = {}, Proj proj = {}) {
    auto first = ranges::begin(range);
    auto last = ranges::end(range);

    /* Calculate size by iterating */
    ptrdiff_t len = 0;
    for (auto it = first; it != last; ++it) {
        ++len;
    }

    while (len > 0) {
        ptrdiff_t half = len / 2;
        auto mid = first;
        for (ptrdiff_t i = 0; i < half; ++i) ++mid;

        if (!comp(value, std::invoke(proj, *mid))) {
            first = mid;
            ++first;
            len = len - half - 1;
        } else {
            len = half;
        }
    }

    return first;
}

template<typename I, typename S, typename T, typename Comp = less<>,
         typename Proj = identity,
         enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                         detail::value_compare_valid<Comp, Proj, I, T>::value,
                     int> = 0>
constexpr subrange<I, S> equal_range(I first, S last, const T& value,
                                    Comp comp = {}, Proj proj = {}) {
    I lower = ranges::lower_bound(first, last, value, comp, proj);
    I upper = ranges::upper_bound(lower, last, value, comp, proj);
    return {lower, upper};
}

template<typename R, typename T, typename Comp = less<>, typename Proj = identity,
         enable_if_t<range<R> && detail::value_compare_valid<Comp, Proj,
                             iterator_t<R>, T>::value, int> = 0>
constexpr auto equal_range(R&& range, const T& value, Comp comp = {},
                           Proj proj = {}) {
    auto result = ranges::equal_range(ranges::begin(range), ranges::end(range),
                                      value, std::move(comp), std::move(proj));
    if constexpr (borrowed_range<R>) return result;
    else return dangling(result.begin());
}

/* ???????????????????????????????????????????????????????????????
 * ?????? |
 * ???????????????????????????????????????????????????????????????*/

template<typename R, typename F>
    requires views::transform_viewable<R, F>
constexpr auto operator|(R&& r, views::transform_adaptor<F> adaptor) {
    return adaptor(std::forward<R>(r));
}

template<typename R, typename Pred>
    requires views::filter_viewable<R, Pred>
constexpr auto operator|(R&& r, views::filter_adaptor<Pred> adaptor) {
    return adaptor(std::forward<R>(r));
}

template<typename R>
    requires views::take_viewable<R>
constexpr auto operator|(R&& r, views::take_adaptor adaptor) {
    return adaptor(std::forward<R>(r));
}

template<typename R>
    requires views::drop_viewable<R>
constexpr auto operator|(R&& r, views::drop_adaptor adaptor) {
    return adaptor(std::forward<R>(r));
}

template<typename R>
    requires views::reverse_viewable<R>
constexpr auto operator|(R&& r, views::reverse_fn) {
    return views::reverse(std::forward<R>(r));
}

#if __cplusplus > 202002L
template<typename R>
    requires views::as_const_viewable<R>
constexpr auto operator|(R&& r, views::as_const_fn) {
    return views::as_const(std::forward<R>(r));
}

template<typename R>
constexpr auto operator|(R&& r, views::chunk_adaptor adaptor) {
    return adaptor(std::forward<R>(r));
}
#endif

template<typename R>
    requires views::all_viewable<R>
constexpr auto operator|(R&& r, views::all_fn) {
    return views::all(std::forward<R>(r));
}

/* ???????????????????????????????????????????????????????????????
 * ranges search/order/transform algorithms
 * ???????????????????????????????????????????????????????????????*/

struct find_first_of_fn {
    template<typename I1, typename S1, typename I2, typename S2,
             typename Pred = equal_to<>, typename Proj1 = identity,
             typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             forward_iterator<I2> && sentinel_for<S2, I2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2, I1, I2>::value,
                         int> = 0>
    constexpr I1 operator()(I1 first1, S1 last1, I2 first2, S2 last2,
                            Pred pred = {}, Proj1 proj1 = {},
                            Proj2 proj2 = {}) const {
        for (; first1 != last1; ++first1) {
            for (I2 cursor = first2; cursor != last2; ++cursor) {
                if (static_cast<bool>(std::invoke(
                        pred, std::invoke(proj1, *first1),
                        std::invoke(proj2, *cursor)))) {
                    return first1;
                }
            }
        }
        return first1;
    }

    template<typename R1, typename R2, typename Pred = equal_to<>,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && forward_range<R2> &&
                             detail::binary_predicate_valid<
                                 Pred, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R1> operator()(R1&& range1, R2&& range2,
                                                 Pred pred = {},
                                                 Proj1 proj1 = {},
                                                 Proj2 proj2 = {}) const {
        auto result = (*this)(ranges::begin(range1), ranges::end(range1),
                              ranges::begin(range2), ranges::end(range2),
                              std::move(pred), std::move(proj1),
                              std::move(proj2));
        if constexpr (borrowed_range<R1>) return result;
        else return dangling(result);
    }
};

inline constexpr find_first_of_fn find_first_of{};

template<typename I>
struct minmax_result {
    I min;
    I max;

    template<typename I2>
    requires convertible_to<const I&, I2>
    constexpr operator minmax_result<I2>() const & {
        return {min, max};
    }

    template<typename I2>
    requires convertible_to<I, I2>
    constexpr operator minmax_result<I2>() && {
        return {std::move(min), std::move(max)};
    }
};

namespace detail {

template<typename Comp, typename Proj, typename I, typename O,
         typename = void>
struct partial_sort_copy_valid : false_type {};

template<typename Comp, typename Proj, typename I, typename O>
struct partial_sort_copy_valid<
    Comp, Proj, I, O,
    void_t<decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj&>(), *declval<I&>()),
               std::invoke(declval<Proj&>(), *declval<O&>()))),
           decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj&>(), *declval<O&>()),
               std::invoke(declval<Proj&>(), *declval<I&>()))),
           decltype(std::invoke(
               declval<Comp&>(),
               std::invoke(declval<Proj&>(), *declval<O&>()),
               std::invoke(declval<Proj&>(), *declval<O&>())))>>
    : integral_constant<bool,
          is_convertible<decltype(std::invoke(
              declval<Comp&>(),
              std::invoke(declval<Proj&>(), *declval<I&>()),
              std::invoke(declval<Proj&>(), *declval<O&>()))), bool>::value &&
          is_convertible<decltype(std::invoke(
              declval<Comp&>(),
              std::invoke(declval<Proj&>(), *declval<O&>()),
              std::invoke(declval<Proj&>(), *declval<I&>()))), bool>::value &&
          is_convertible<decltype(std::invoke(
              declval<Comp&>(),
              std::invoke(declval<Proj&>(), *declval<O&>()),
              std::invoke(declval<Proj&>(), *declval<O&>()))), bool>::value> {};

} /* namespace detail */

struct partial_sort_copy_fn {
    template<typename I, typename S, typename O, typename OS,
             typename Comp = less<>, typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             random_access_iterator<O> && sentinel_for<OS, O> &&
                             detail::copy_assignment_valid<I, O>::value &&
                             detail::swap_ranges_valid<O, O>::value &&
                             detail::partial_sort_copy_valid<
                                 Comp, Proj, I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result_first,
                                             OS result_last,
                                             Comp comp = {}, Proj proj = {}) const {
        O result_end = result_first;
        while (result_end != result_last) ++result_end;
        O output = result_first;
        for (; first != last && output != result_end; ++first, ++output) {
            *output = *first;
        }
        O selected_end = output;
        const auto selected = selected_end - result_first;
        if (selected == 0) {
            while (first != last) ++first;
            return {first, selected_end};
        }

        auto less_projected = [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(
                comp, std::invoke(proj, left), std::invoke(proj, right)));
        };
        std::make_heap(result_first, selected_end, less_projected);
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, *result_first)))) {
                *result_first = *first;
                std::make_heap(result_first, selected_end, less_projected);
            }
        }
        std::sort(result_first, selected_end, less_projected);
        return {first, selected_end};
    }

    template<typename R, typename O, typename OS, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<input_range<R> && random_access_iterator<O> &&
                             sentinel_for<OS, O> &&
                             detail::copy_assignment_valid<
                                 iterator_t<R>, O>::value &&
                             detail::swap_ranges_valid<O, O>::value &&
                             detail::partial_sort_copy_valid<
                                 Comp, Proj, iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result_first, OS result_last, Comp comp = {},
               Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              result_first, result_last, std::move(comp),
                              std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {result.in, result.out};
        } else {
            return {dangling(result.in), result.out};
        }
    }
};

inline constexpr partial_sort_copy_fn partial_sort_copy{};

struct min_fn {
    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr const T& operator()(const T& left, const T& right,
                                  Comp comp = {}, Proj proj = {}) const {
        return static_cast<bool>(std::invoke(
            comp, std::invoke(proj, right), std::invoke(proj, left)))
                   ? right
                   : left;
    }

    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr T operator()(initializer_list<T> values, Comp comp = {},
                           Proj proj = {}) const {
        if (values.size() == 0) __builtin_trap();
        auto first = values.begin();
        auto result = *first++;
        for (; first != values.end(); ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, result)))) {
                result = *first;
            }
        }
        return result;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<input_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr range_value_t<R> operator()(R&& range, Comp comp = {},
                                           Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first == last) __builtin_trap();
        range_value_t<R> result = *first++;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, result)))) {
                result = *first;
            }
        }
        return result;
    }
};

inline constexpr min_fn min{};

struct max_fn {
    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr const T& operator()(const T& left, const T& right,
                                  Comp comp = {}, Proj proj = {}) const {
        return static_cast<bool>(std::invoke(
            comp, std::invoke(proj, left), std::invoke(proj, right)))
                   ? right
                   : left;
    }

    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr T operator()(initializer_list<T> values, Comp comp = {},
                           Proj proj = {}) const {
        if (values.size() == 0) __builtin_trap();
        auto first = values.begin();
        auto result = *first++;
        for (; first != values.end(); ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, result),
                    std::invoke(proj, *first)))) {
                result = *first;
            }
        }
        return result;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<input_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr range_value_t<R> operator()(R&& range, Comp comp = {},
                                           Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first == last) __builtin_trap();
        range_value_t<R> result = *first++;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, result),
                    std::invoke(proj, *first)))) {
                result = *first;
            }
        }
        return result;
    }
};

inline constexpr max_fn max{};

struct minmax_fn {
    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr minmax_result<const T&> operator()(const T& left,
                                                 const T& right,
                                                 Comp comp = {},
                                                 Proj proj = {}) const {
        const T* minimum = &left;
        const T* maximum = &right;
        if (static_cast<bool>(std::invoke(
                comp, std::invoke(proj, right), std::invoke(proj, left)))) {
            minimum = &right;
        }
        if (static_cast<bool>(std::invoke(
                comp, std::invoke(proj, right), std::invoke(proj, left)))) {
            maximum = &left;
        }
        return {*minimum, *maximum};
    }

    template<typename T, typename Comp = less<>, typename Proj = identity,
             enable_if_t<detail::value_compare_valid<
                                 Comp, Proj, const T*, T>::value,
                         int> = 0>
    constexpr minmax_result<T> operator()(initializer_list<T> values,
                                          Comp comp = {}, Proj proj = {}) const {
        if (values.size() == 0) __builtin_trap();
        auto first = values.begin();
        T minimum = *first;
        T maximum = *first++;
        for (; first != values.end(); ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, minimum)))) {
                minimum = *first;
            }
            if (!static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, maximum)))) {
                maximum = *first;
            }
        }
        return {minimum, maximum};
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<input_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr minmax_result<range_value_t<R>> operator()(R&& range,
                                                         Comp comp = {},
                                                         Proj proj = {}) const {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        if (first == last) __builtin_trap();
        range_value_t<R> minimum = *first;
        range_value_t<R> maximum = minimum;
        ++first;
        for (; first != last; ++first) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, minimum)))) {
                minimum = *first;
            }
            if (!static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *first),
                    std::invoke(proj, maximum)))) {
                maximum = *first;
            }
        }
        return {minimum, maximum};
    }
};

inline constexpr minmax_fn minmax{};

struct min_element_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {},
                           Proj proj = {}) const {
        if (first == last) return first;
        I result = first;
        I cursor = first;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *cursor),
                    std::invoke(proj, *result)))) {
                result = cursor;
            }
        }
        return result;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr min_element_fn min_element{};

struct max_element_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {},
                           Proj proj = {}) const {
        if (first == last) return first;
        I result = first;
        I cursor = first;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *result),
                    std::invoke(proj, *cursor)))) {
                result = cursor;
            }
        }
        return result;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr max_element_fn max_element{};

struct minmax_element_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<forward_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value,
                         int> = 0>
    constexpr minmax_result<I> operator()(I first, S last, Comp comp = {},
                                          Proj proj = {}) const {
        if (first == last) return {first, first};
        I minimum = first;
        I maximum = first;
        I cursor = first;
        ++cursor;
        for (; cursor != last; ++cursor) {
            if (static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *cursor),
                    std::invoke(proj, *minimum)))) {
                minimum = cursor;
            }
            if (!static_cast<bool>(std::invoke(
                    comp, std::invoke(proj, *cursor),
                    std::invoke(proj, *maximum)))) {
                maximum = cursor;
            }
        }
        return {minimum, maximum};
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<forward_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value,
                         int> = 0>
    constexpr auto operator()(R&& range, Comp comp = {}, Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return result;
        } else {
            return minmax_result<dangling>{dangling(result.min),
                                           dangling(result.max)};
        }
    }
};

inline constexpr minmax_element_fn minmax_element{};

struct sort_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {},
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        std::sort(first, end, [&](const auto& left, const auto& right) {
            return static_cast<bool>(std::invoke(
                comp, std::invoke(proj, left), std::invoke(proj, right)));
        });
        return end;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr sort_fn sort{};

struct stable_sort_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, S last, Comp comp = {},
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        std::stable_sort(first, end,
                         [&](const auto& left, const auto& right) {
                             return static_cast<bool>(std::invoke(
                                 comp, std::invoke(proj, left),
                                 std::invoke(proj, right)));
                         });
        return end;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range, Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr stable_sort_fn stable_sort{};

struct partial_sort_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, I middle, S last, Comp comp = {},
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        std::partial_sort(first, middle, end,
                          [&](const auto& left, const auto& right) {
                              return static_cast<bool>(std::invoke(
                                  comp, std::invoke(proj, left),
                                  std::invoke(proj, right)));
                          });
        return end;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range,
                                                 iterator_t<R> middle,
                                                 Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), middle, ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr partial_sort_fn partial_sort{};

struct nth_element_fn {
    template<typename I, typename S, typename Comp = less<>,
             typename Proj = identity,
             enable_if_t<random_access_iterator<I> && sentinel_for<S, I> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj, I, I>::value &&
                             detail::swap_ranges_valid<I, I>::value,
                         int> = 0>
    constexpr I operator()(I first, I nth, S last, Comp comp = {},
                           Proj proj = {}) const {
        I end = first;
        while (end != last) ++end;
        std::nth_element(first, nth, end,
                         [&](const auto& left, const auto& right) {
                             return static_cast<bool>(std::invoke(
                                 comp, std::invoke(proj, left),
                                 std::invoke(proj, right)));
                         });
        return end;
    }

    template<typename R, typename Comp = less<>, typename Proj = identity,
             enable_if_t<random_access_range<R> &&
                             detail::binary_predicate_valid<
                                 Comp, Proj, Proj,
                                 iterator_t<R>, iterator_t<R>>::value &&
                             detail::swap_ranges_valid<iterator_t<R>,
                                 iterator_t<R>>::value,
                         int> = 0>
    constexpr borrowed_iterator_t<R> operator()(R&& range,
                                                 iterator_t<R> nth,
                                                 Comp comp = {},
                                                 Proj proj = {}) const {
        auto result = (*this)(ranges::begin(range), nth, ranges::end(range),
                              std::move(comp), std::move(proj));
        if constexpr (borrowed_range<R>) return result;
        else return dangling(result);
    }
};

inline constexpr nth_element_fn nth_element{};

namespace detail {

template<typename F, typename Proj, typename I, typename O,
         typename = void>
struct transform_unary_valid : false_type {};

template<typename F, typename Proj, typename I, typename O>
struct transform_unary_valid<
    F, Proj, I, O,
    void_t<decltype(*declval<O&>() = std::invoke(
        declval<F&>(), std::invoke(declval<Proj&>(), *declval<I&>())))>>
    : true_type {};

template<typename F, typename Proj1, typename Proj2, typename I1,
         typename I2, typename O, typename = void>
struct transform_binary_valid : false_type {};

template<typename F, typename Proj1, typename Proj2, typename I1,
         typename I2, typename O>
struct transform_binary_valid<
    F, Proj1, Proj2, I1, I2, O,
    void_t<decltype(*declval<O&>() = std::invoke(
        declval<F&>(),
        std::invoke(declval<Proj1&>(), *declval<I1&>()),
        std::invoke(declval<Proj2&>(), *declval<I2&>())))>>
    : true_type {};

} /* namespace detail */

struct transform_fn {
    template<typename I, typename S, typename O, typename F,
             typename Proj = identity,
             enable_if_t<input_iterator<I> && sentinel_for<S, I> &&
                             input_or_output_iterator<O> &&
                             detail::transform_unary_valid<
                                 F, Proj, I, O>::value,
                         int> = 0>
    constexpr in_out_result<I, O> operator()(I first, S last, O result, F op,
                                             Proj proj = {}) const {
        for (; first != last; ++first, ++result) {
            *result = std::invoke(op, std::invoke(proj, *first));
        }
        return {first, result};
    }

    template<typename R, typename O, typename F, typename Proj = identity,
             enable_if_t<input_range<R> && input_or_output_iterator<O> &&
                             detail::transform_unary_valid<
                                 F, Proj, iterator_t<R>, O>::value,
                         int> = 0>
    constexpr in_out_result<borrowed_iterator_t<R>, O>
    operator()(R&& range, O result, F op, Proj proj = {}) const {
        auto transformed = (*this)(ranges::begin(range), ranges::end(range),
                                   result, std::move(op), std::move(proj));
        if constexpr (borrowed_range<R>) {
            return {transformed.in, transformed.out};
        } else {
            return {dangling(transformed.in), transformed.out};
        }
    }

    template<typename I1, typename S1, typename I2, typename S2, typename O,
             typename F, typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_iterator<I1> && sentinel_for<S1, I1> &&
                             input_iterator<I2> && sentinel_for<S2, I2> &&
                             input_or_output_iterator<O> &&
                             detail::transform_binary_valid<
                                 F, Proj1, Proj2, I1, I2, O>::value,
                         int> = 0>
    constexpr in_in_out_result<I1, I2, O>
    operator()(I1 first1, S1 last1, I2 first2, S2 last2, O result, F op,
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        while (first1 != last1 && first2 != last2) {
            *result = std::invoke(op, std::invoke(proj1, *first1),
                                  std::invoke(proj2, *first2));
            ++first1;
            ++first2;
            ++result;
        }
        return {first1, first2, result};
    }

    template<typename R1, typename R2, typename O, typename F,
             typename Proj1 = identity, typename Proj2 = identity,
             enable_if_t<input_range<R1> && input_range<R2> &&
                             input_or_output_iterator<O> &&
                             detail::transform_binary_valid<
                                 F, Proj1, Proj2,
                                 iterator_t<R1>, iterator_t<R2>, O>::value,
                         int> = 0>
    constexpr in_in_out_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
    operator()(R1&& range1, R2&& range2, O result, F op,
               Proj1 proj1 = {}, Proj2 proj2 = {}) const {
        auto transformed = (*this)(ranges::begin(range1), ranges::end(range1),
                                   ranges::begin(range2), ranges::end(range2),
                                   result, std::move(op), std::move(proj1),
                                   std::move(proj2));
        return make_in_in_out_result<R1, R2>(transformed);
    }
};

inline constexpr transform_fn transform{};

} /* namespace ranges */

template<typename I, typename S>
struct tuple_size<ranges::subrange<I, S>> : integral_constant<size_t, 2> {};

template<typename I, typename S>
struct tuple_size<const ranges::subrange<I, S>>
    : integral_constant<size_t, 2> {};

template<typename I, typename S>
struct tuple_size<volatile ranges::subrange<I, S>>
    : integral_constant<size_t, 2> {};

template<typename I, typename S>
struct tuple_size<const volatile ranges::subrange<I, S>>
    : integral_constant<size_t, 2> {};

template<size_t Index, typename I, typename S>
struct tuple_element<Index, ranges::subrange<I, S>> {
    static_assert(Index < 2, "subrange tuple element index out of bounds");
    using type = conditional_t<Index == 0, I, S>;
};

template<size_t Index, typename I, typename S>
struct tuple_element<Index, const ranges::subrange<I, S>> {
    static_assert(Index < 2, "subrange tuple element index out of bounds");
    using type = const conditional_t<Index == 0, I, S>;
};

template<size_t Index, typename I, typename S>
struct tuple_element<Index, volatile ranges::subrange<I, S>> {
    static_assert(Index < 2, "subrange tuple element index out of bounds");
    using type = volatile conditional_t<Index == 0, I, S>;
};

template<size_t Index, typename I, typename S>
struct tuple_element<Index, const volatile ranges::subrange<I, S>> {
    static_assert(Index < 2, "subrange tuple element index out of bounds");
    using type = const volatile conditional_t<Index == 0, I, S>;
};

#if __cplusplus > 202002L
namespace detail {

template<typename R, typename T>
concept container_compatible_range =
    ranges::input_range<R> && convertible_to<ranges::range_reference_t<R>, T>;

/* A sized range can prove emptiness without touching its input iterator.  The
 * adaptor push_range operations use this narrow helper before constructing a
 * transactional candidate, so an empty vector/array does not allocate or
 * copy the existing adaptor state.  Unsized single-pass ranges deliberately
 * return false: probing begin/end would consume or otherwise perturb some
 * input views before the actual append. */
template<typename R>
constexpr bool container_range_known_empty_impl(R&& range, true_type) {
    return ranges::size(range) == 0;
}

template<typename R>
constexpr bool container_range_known_empty_impl(R&&, false_type) {
    return false;
}

template<typename R>
constexpr bool container_range_known_empty(R&& range) {
    return container_range_known_empty_impl(
        std::forward<R>(range),
        integral_constant<bool, ranges::sized_range<R>>());
}

template<typename Container, typename R>
constexpr void append_container_range(Container& container, R&& range) {
    if constexpr (requires {
                      container.append_range(std::forward<R>(range));
                  }) {
        container.append_range(std::forward<R>(range));
    } else {
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) {
            container.push_back(*first);
        }
    }
}

template<typename Container, typename R>
constexpr Container make_container_from_range(R&& range) {
    if constexpr (requires {
                      Container(from_range, std::forward<R>(range));
                  }) {
        return Container(from_range, std::forward<R>(range));
    } else if constexpr (requires {
                             Container(ranges::begin(range),
                                       ranges::end(range));
                         }) {
        return Container(ranges::begin(range), ranges::end(range));
    } else {
        Container container;
        append_container_range(container, std::forward<R>(range));
        return container;
    }
}

template<typename Container, typename R, typename Tuple, size_t... Is>
constexpr Container make_container_from_range_args(
    R&& range, Tuple&& args, index_sequence<Is...>) {
    if constexpr (requires {
                      Container(from_range, std::forward<R>(range),
                                std::get<Is>(std::forward<Tuple>(args))...);
                  }) {
        return Container(from_range, std::forward<R>(range),
                         std::get<Is>(std::forward<Tuple>(args))...);
    } else if constexpr (requires {
                             Container(
                                 ranges::begin(range), ranges::end(range),
                                 std::get<Is>(std::forward<Tuple>(args))...);
                         }) {
        return Container(
            ranges::begin(range), ranges::end(range),
            std::get<Is>(std::forward<Tuple>(args))...);
    } else {
        Container container(
            std::get<Is>(std::forward<Tuple>(args))...);
        append_container_range(container, std::forward<R>(range));
        return container;
    }
}
template<typename Container, typename R, typename Alloc>
constexpr Container make_container_from_range(R&& range, const Alloc& alloc) {
    if constexpr (requires {
                      Container(from_range, std::forward<R>(range), alloc);
                  }) {
        return Container(from_range, std::forward<R>(range), alloc);
    } else if constexpr (requires {
                             Container(ranges::begin(range),
                                       ranges::end(range), alloc);
                         }) {
        return Container(ranges::begin(range), ranges::end(range), alloc);
    } else {
        Container container(alloc);
        append_container_range(container, std::forward<R>(range));
        return container;
    }
}

} /* namespace detail */
#endif

/* ???????????????????????????????????????????????????????????????
 * std???????????
 * ???????????????????????????????????????????????????????????????*/

namespace ranges {

/* basic_istream_view is a single-pass range whose iterator owns the current
 * extracted value.  Extraction is attempted only by begin() and increment();
 * a stream failure transitions the cursor to the default sentinel without
 * publishing a partially parsed value. */
template<typename T, typename CharT = char,
         typename Traits = char_traits<CharT>>
    requires default_initializable<T>
class basic_istream_view {
    basic_istream<CharT, Traits>* stream_;

public:
    class iterator {
        basic_istream<CharT, Traits>* stream_;
        T value_;

        constexpr void read() {
            if (!stream_) return;
            *stream_ >> value_;
            if (stream_->fail()) stream_ = nullptr;
        }

    public:
        using iterator_category = input_iterator_tag;
        using iterator_concept = input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        constexpr iterator() : stream_(nullptr), value_() {}
        explicit iterator(basic_istream<CharT, Traits>& stream)
            : stream_(&stream), value_() { read(); }

        reference operator*() const { return value_; }
        pointer operator->() const { return &value_; }

        iterator& operator++() { read(); return *this; }
        iterator operator++(int) {
            iterator before(*this);
            ++(*this);
            return before;
        }

        friend bool operator==(const iterator& left, const iterator& right) {
            return left.stream_ == right.stream_;
        }
        friend bool operator!=(const iterator& left, const iterator& right) {
            return !(left == right);
        }
        friend bool operator==(const iterator& value, default_sentinel_t) {
            return value.stream_ == nullptr;
        }
        friend bool operator==(default_sentinel_t sentinel, const iterator& value) {
            return value == sentinel;
        }
        friend bool operator!=(const iterator& value, default_sentinel_t sentinel) {
            return !(value == sentinel);
        }
        friend bool operator!=(default_sentinel_t sentinel, const iterator& value) {
            return !(sentinel == value);
        }
    };

    explicit basic_istream_view(basic_istream<CharT, Traits>& stream)
        : stream_(&stream) {}

    iterator begin() { return iterator(*stream_); }
    default_sentinel_t end() const noexcept { return default_sentinel; }
};

namespace views {
template<typename T>
struct istream_fn {
    template<typename CharT, typename Traits>
    constexpr auto operator()(basic_istream<CharT, Traits>& stream) const
        -> basic_istream_view<T, CharT, Traits> {
        return basic_istream_view<T, CharT, Traits>(stream);
    }
};
} /* namespace views */

} /* namespace ranges */

namespace ranges::views {
template<typename T>
inline constexpr istream_fn<T> istream{};
}

namespace views = ranges::views;

} /* namespace std */

#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_RANGES_H */
