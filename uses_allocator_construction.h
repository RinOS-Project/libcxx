/*
 * Allocator-aware construction utilities for <memory>.
 */

#ifndef RINCXX_USES_ALLOCATOR_CONSTRUCTION_H
#define RINCXX_USES_ALLOCATOR_CONSTRUCTION_H

#include "memory.h"
#include "tuple.h"
#include "array.h"

#if __cplusplus >= 202002L

namespace std {
namespace detail {

template<typename T> struct is_allocator_construction_pair : false_type {};
template<typename First, typename Second>
struct is_allocator_construction_pair<pair<First, Second>> : true_type {};

template<size_t Index, typename Tuple>
constexpr auto allocator_construction_get(Tuple&& value)
    -> decltype(get<Index>(std::forward<Tuple>(value))) {
    return get<Index>(std::forward<Tuple>(value));
}

template<typename Tuple, typename = void>
struct is_allocator_construction_pair_like : false_type {};

template<typename Tuple>
struct is_allocator_construction_pair_like<
    Tuple,
    void_t<decltype(tuple_size<remove_cvref_t<Tuple>>::value),
           decltype(allocator_construction_get<0>(declval<Tuple>())),
           decltype(allocator_construction_get<1>(declval<Tuple>()))>>
    : integral_constant<bool,
          tuple_size<remove_cvref_t<Tuple>>::value == 2> {};

template<typename Pair, typename Alloc, typename Source>
struct allocator_pair_constructor {
    using pair_type = remove_cv_t<Pair>;
    const Alloc& alloc;
    Source& source;

    constexpr operator pair_type() const;
};

} // namespace detail

template<typename T, typename Alloc, typename... Args,
         typename enable_if<
             !detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, Args&&... args) noexcept;

template<typename T, typename Alloc, typename Tuple1, typename Tuple2,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, piecewise_construct_t,
    Tuple1&& first_args, Tuple2&& second_args) noexcept;

template<typename T, typename Alloc,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc) noexcept;

template<typename T, typename Alloc, typename First, typename Second,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, First&& first, Second&& second) noexcept;

template<typename T, typename Alloc, typename PairLike,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value &&
                 detail::is_allocator_construction_pair_like<PairLike&&>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, PairLike&& value) noexcept;

template<typename T, typename Alloc, typename Source,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value &&
                 !detail::is_allocator_construction_pair_like<Source&&>::value,
             int>::type = 0>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, Source&& value) noexcept;

template<typename T, typename Alloc, typename... Args>
constexpr T make_obj_using_allocator(const Alloc& alloc, Args&&... args);

namespace detail {

template<typename T, typename Alloc, typename... Args,
         typename Leading, typename Trailing>
constexpr auto allocator_construction_args(
    const Alloc&, false_type, Leading, Trailing,
    Args&&... args) noexcept {
    static_assert(is_constructible<T, Args&&...>::value,
                  "T is not constructible from the supplied arguments");
    return forward_as_tuple(std::forward<Args>(args)...);
}

template<typename T, typename Alloc, typename... Args>
constexpr auto allocator_construction_args(
    const Alloc& alloc, true_type, true_type, true_type,
    Args&&... args) noexcept {
    (void)alloc;
    return tuple<allocator_arg_t, const Alloc&, Args&&...>(
        allocator_arg, alloc, std::forward<Args>(args)...);
}

template<typename T, typename Alloc, typename... Args>
constexpr auto allocator_construction_args(
    const Alloc& alloc, true_type, true_type, false_type,
    Args&&... args) noexcept {
    (void)alloc;
    return tuple<allocator_arg_t, const Alloc&, Args&&...>(
        allocator_arg, alloc, std::forward<Args>(args)...);
}

template<typename T, typename Alloc, typename... Args>
constexpr auto allocator_construction_args(
    const Alloc& alloc, true_type, false_type, true_type,
    Args&&... args) noexcept {
    return forward_as_tuple(std::forward<Args>(args)..., alloc);
}

template<typename T, typename Alloc, typename... Args>
constexpr auto allocator_construction_args(
    const Alloc&, true_type, false_type, false_type,
    Args&&...) noexcept {
    static_assert(!uses_allocator<remove_cv_t<T>, Alloc>::value,
                  "allocator-aware T has no compatible allocator constructor");
    return tuple<>();
}

template<typename T, typename Alloc, typename Tuple1, typename Tuple2>
constexpr auto allocator_pair_piecewise_args(
    const Alloc& alloc, Tuple1&& first_args, Tuple2&& second_args) noexcept {
    using pair_type = remove_cv_t<T>;
    using first_type = typename pair_type::first_type;
    using second_type = typename pair_type::second_type;
    auto first = apply(
        [&alloc](auto&&... args) {
            return uses_allocator_construction_args<first_type>(
                alloc, std::forward<decltype(args)>(args)...);
        }, std::forward<Tuple1>(first_args));
    auto second = apply(
        [&alloc](auto&&... args) {
            return uses_allocator_construction_args<second_type>(
                alloc, std::forward<decltype(args)>(args)...);
        }, std::forward<Tuple2>(second_args));
    return make_tuple(piecewise_construct, std::move(first),
                      std::move(second));
}

} // namespace detail

template<typename T, typename Alloc, typename... Args,
         typename enable_if<
             !detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, Args&&... args) noexcept {
    using target_type = remove_cv_t<T>;
    return detail::allocator_construction_args<T>(
        alloc,
        integral_constant<bool, uses_allocator<target_type, Alloc>::value>(),
        integral_constant<bool, is_constructible<
            T, allocator_arg_t, const Alloc&, Args&&...>::value>(),
        integral_constant<bool, is_constructible<
            T, Args&&..., const Alloc&>::value>(),
        std::forward<Args>(args)...);
}

template<typename T, typename Alloc, typename Tuple1, typename Tuple2,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, piecewise_construct_t,
    Tuple1&& first_args, Tuple2&& second_args) noexcept {
    return detail::allocator_pair_piecewise_args<T>(
        alloc, std::forward<Tuple1>(first_args),
        std::forward<Tuple2>(second_args));
}

template<typename T, typename Alloc,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc) noexcept {
    return uses_allocator_construction_args<T>(
        alloc, piecewise_construct, tuple<>(), tuple<>());
}

template<typename T, typename Alloc, typename First, typename Second,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, First&& first, Second&& second) noexcept {
    return uses_allocator_construction_args<T>(
        alloc, piecewise_construct,
        forward_as_tuple(std::forward<First>(first)),
        forward_as_tuple(std::forward<Second>(second)));
}

template<typename T, typename Alloc, typename PairLike,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value &&
                 detail::is_allocator_construction_pair_like<PairLike&&>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, PairLike&& value) noexcept {
    return uses_allocator_construction_args<T>(
        alloc, piecewise_construct,
        forward_as_tuple(detail::allocator_construction_get<0>(
            std::forward<PairLike>(value))),
        forward_as_tuple(detail::allocator_construction_get<1>(
            std::forward<PairLike>(value))));
}

template<typename T, typename Alloc, typename Source,
         typename enable_if<
             detail::is_allocator_construction_pair<remove_cv_t<T>>::value &&
                 !detail::is_allocator_construction_pair_like<Source&&>::value,
             int>::type>
constexpr auto uses_allocator_construction_args(
    const Alloc& alloc, Source&& value) noexcept {
    using pair_type = remove_cv_t<T>;
    static_assert(is_constructible<pair_type, Source&&>::value,
                  "pair is not constructible from the supplied value");
    using constructor = detail::allocator_pair_constructor<
        T, Alloc, Source>;
    return make_tuple(constructor{alloc, value});
}

template<typename T, typename Alloc, typename... Args>
constexpr T make_obj_using_allocator(const Alloc& alloc, Args&&... args) {
    return make_from_tuple<T>(uses_allocator_construction_args<T>(
        alloc, std::forward<Args>(args)...));
}

template<typename Pair, typename Alloc, typename Source>
constexpr detail::allocator_pair_constructor<
    Pair, Alloc, Source>::operator typename detail::allocator_pair_constructor<
        Pair, Alloc, Source>::pair_type() const {
    using pair_type = typename detail::allocator_pair_constructor<
        Pair, Alloc, Source>::pair_type;
    return make_obj_using_allocator<pair_type>(
        alloc, static_cast<pair_type>(std::forward<Source>(source)));
}

template<typename T, typename Alloc, typename... Args>
constexpr T* uninitialized_construct_using_allocator(
    T* location, const Alloc& alloc, Args&&... args) {
    return apply(
        [location](auto&&... forwarded) {
            return construct_at(
                location, std::forward<decltype(forwarded)>(forwarded)...);
        },
        uses_allocator_construction_args<T>(
            alloc, std::forward<Args>(args)...));
}

} // namespace std

#endif // __cplusplus >= 202002L

#endif // RINCXX_USES_ALLOCATOR_CONSTRUCTION_H
