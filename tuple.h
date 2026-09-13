/*
 * RinOS C++ Tuple ✿
 * std::tuple 互換実装
 */

#ifndef RINCXX_TUPLE_H
#define RINCXX_TUPLE_H

#include "rincxx.h"
#include "type_traits.h"
#include "utility.h"  /* index_sequence, make_index_sequence */
#include "functional.h"  /* reference_wrapper, invoke */
#include "memory.h"  /* allocator_arg_t, uses_allocator */
#include "__tuple_fwd.h"
#if __cplusplus >= 202002L
#include "compare.h"
#endif

#if __cplusplus >= 201402L
#define RIN_TUPLE_CONSTEXPR14 constexpr
#else
#define RIN_TUPLE_CONSTEXPR14 inline
#endif

#ifdef __cplusplus

namespace std {

template<typename... Types>
class tuple;

template<typename T1, typename T2>
struct pair;

/* Allocator-aware tuple construction is defined before the concrete get
 * overloads below.  These declarations keep source-tuple expansion dependent
 * until tuple_element and tuple_leaf are complete. */
template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&
get(tuple<Types...>& t) noexcept;

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&
get(const tuple<Types...>& t) noexcept;

template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&&
get(tuple<Types...>&& t) noexcept;

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&&
get(const tuple<Types...>&& t) noexcept;

namespace detail {

template<typename T> struct is_tuple_specialization : false_type {};
template<typename... Ts>
struct is_tuple_specialization<tuple<Ts...>> : true_type {};

template<typename T> struct is_pair_specialization : false_type {};
template<typename T1, typename T2>
struct is_pair_specialization<pair<T1, T2>> : true_type {};

/* The C++23 tuple-like constructors accept tuple_size/get customization as
 * well as the standard array and subrange sources.  tuple and pair retain
 * their C++11 constructor families below.  The standard concept is a closed
 * set, but RinOS deliberately supports conforming user-defined tuple-like
 * types so that existing ADL get customizations compose with the tuple
 * conversion and assignment matrix. */
template<typename T> struct is_additional_tuple_like : false_type {};
template<typename T, size_t N>
struct is_additional_tuple_like<array<T, N>> : true_type {};
#if __cplusplus > 202002L
template<typename I, typename S>
struct is_additional_tuple_like<ranges::subrange<I, S>> : true_type {};
#endif

template<typename Destination, typename Source, bool SameArity>
struct tuple_pack_constructible_impl : false_type {};

template<typename... Destination, typename... Source>
struct tuple_pack_constructible_impl<
    tuple<Destination...>, tuple<Source...>, true>
    : conjunction<is_constructible<Destination, Source>...> {};

template<typename Destination, typename Source>
struct tuple_pack_constructible;

template<typename... Destination, typename... Source>
struct tuple_pack_constructible<tuple<Destination...>, tuple<Source...>>
    : tuple_pack_constructible_impl<
          tuple<Destination...>, tuple<Source...>,
          sizeof...(Destination) == sizeof...(Source)> {};

template<typename Destination, typename Source, bool SameArity>
struct tuple_pack_convertible_impl : false_type {};

template<typename... Destination, typename... Source>
struct tuple_pack_convertible_impl<
    tuple<Destination...>, tuple<Source...>, true>
    : conjunction<is_convertible<Source, Destination>...> {};

template<typename Destination, typename Source>
struct tuple_pack_convertible;

template<typename... Destination, typename... Source>
struct tuple_pack_convertible<tuple<Destination...>, tuple<Source...>>
    : tuple_pack_convertible_impl<
          tuple<Destination...>, tuple<Source...>,
          sizeof...(Destination) == sizeof...(Source)> {};

template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_constructible : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_constructible<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_constructible<
          Destination, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_nothrow_constructible : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_nothrow_constructible<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<
          integral_constant<bool,
              noexcept(get<Indices>(declval<Source>()))>...,
          is_nothrow_constructible<
              Destination, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         bool SameArity>
struct tuple_source_constructible_if_same : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_constructible_if_same<
    Destination, Source, Indices, true>
    : tuple_source_constructible<Destination, Source, Indices> {};

template<typename Destination, typename Source, typename Indices,
         bool SameArity>
struct tuple_source_nothrow_constructible_if_same : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_nothrow_constructible_if_same<
    Destination, Source, Indices, true>
    : tuple_source_nothrow_constructible<Destination, Source, Indices> {};

template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_convertible : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_convertible<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_convertible<
          decltype(get<Indices>(declval<Source>())), Destination>...> {};

template<typename Destination, typename Source, typename Indices,
         bool SameArity>
struct tuple_source_convertible_if_same : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_convertible_if_same<
    Destination, Source, Indices, true>
    : tuple_source_convertible<Destination, Source, Indices> {};

template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_assignable : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_assignable<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_assignable<
          Destination&, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         bool Enabled>
struct tuple_source_assignable_if_enabled : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_assignable_if_enabled<
    Destination, Source, Indices, true>
    : tuple_source_assignable<Destination, Source, Indices> {};

/* Converting assignment also carries a conditional-noexcept contract.  Keep
 * the expression probe SFINAE-friendly so malformed tuple-like get<I>
 * customizations simply remove the overload. */
template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_nothrow_assignable : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_nothrow_assignable<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_nothrow_assignable<
          Destination&, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         bool Enabled>
struct tuple_source_nothrow_assignable_if_enabled : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_nothrow_assignable_if_enabled<
    Destination, Source, Indices, true>
    : tuple_source_nothrow_assignable<Destination, Source, Indices> {};

#if __cplusplus > 202002L
template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_const_assignable : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_const_assignable<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_assignable<
          const Destination&, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         bool Enabled>
struct tuple_source_const_assignable_if_enabled : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_const_assignable_if_enabled<
    Destination, Source, Indices, true>
    : tuple_source_const_assignable<Destination, Source, Indices> {};

template<typename Destination, typename Source, typename Indices,
         typename = void>
struct tuple_source_const_nothrow_assignable : false_type {};

template<typename... Destination, typename Source, size_t... Indices>
struct tuple_source_const_nothrow_assignable<
    tuple<Destination...>, Source, index_sequence<Indices...>,
    void_t<decltype(get<Indices>(declval<Source>()))...>>
    : conjunction<is_nothrow_assignable<
          const Destination&, decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Destination, typename Source, typename Indices,
         bool Enabled>
struct tuple_source_const_nothrow_assignable_if_enabled : false_type {};

template<typename Destination, typename Source, typename Indices>
struct tuple_source_const_nothrow_assignable_if_enabled<
    Destination, Source, Indices, true>
    : tuple_source_const_nothrow_assignable<Destination, Source, Indices> {};
#endif

template<typename Source, size_t Arity, typename = void>
struct is_additional_tuple_like_source : false_type {};

template<typename Source, size_t Arity>
struct is_additional_tuple_like_source<
    Source, Arity,
    void_t<decltype(tuple_size<remove_cvref_t<Source>>::value)>>
    : integral_constant<bool,
        /* Keep tuple/pair on their dedicated overload families.  Any other
         * complete tuple_size specialization is accepted here; the
         * element-wise constructible/assignable traits below still require
         * every ADL get<I> expression to be valid, so malformed
         * specializations simply disappear by SFINAE. */
        (!is_tuple_specialization<remove_cvref_t<Source>>::value &&
         !is_pair_specialization<remove_cvref_t<Source>>::value) &&
        tuple_size<remove_cvref_t<Source>>::value == Arity> {};

/* [uses.allocator.construction] has three mutually exclusive paths.  A
 * tuple must use the leading allocator form when it is available; trailing
 * form is considered only when leading form is not constructible. */
template<typename T, typename Alloc, typename... Args>
struct tuple_uses_allocator_constructible
    : integral_constant<bool,
        !uses_allocator<T, Alloc>::value
            ? is_constructible<T, Args&&...>::value
            : (is_constructible<T, allocator_arg_t, const Alloc&,
                                Args&&...>::value ||
               is_constructible<T, Args&&..., const Alloc&>::value)> {};

/* Keep allocator construction's exception contract separate from its
 * participation contract.  Leading allocator construction wins whenever it
 * is viable; trailing construction is considered only as the fallback. */
template<typename T, typename Alloc, typename... Args>
struct tuple_uses_allocator_nothrow_constructible
    : integral_constant<bool,
        !uses_allocator<T, Alloc>::value
            ? is_nothrow_constructible<T, Args&&...>::value
            : (is_constructible<T, allocator_arg_t, const Alloc&,
                                Args&&...>::value
                   ? is_nothrow_constructible<T, allocator_arg_t,
                                               const Alloc&, Args&&...>::value
                   : is_nothrow_constructible<T, Args&&...,
                                               const Alloc&>::value)> {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices>
struct tuple_allocator_source_constructible;

template<typename Alloc, typename... Destination, typename Source,
         size_t... Indices>
struct tuple_allocator_source_constructible<
    Alloc, tuple<Destination...>, Source, index_sequence<Indices...>>
    : conjunction<tuple_uses_allocator_constructible<
          Destination, Alloc,
          decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices>
struct tuple_allocator_source_nothrow_constructible;

template<typename Alloc, typename... Destination, typename Source,
         size_t... Indices>
struct tuple_allocator_source_nothrow_constructible<
    Alloc, tuple<Destination...>, Source, index_sequence<Indices...>>
    : conjunction<
          integral_constant<bool,
              noexcept(get<Indices>(declval<Source>()))>...,
          tuple_uses_allocator_nothrow_constructible<
              Destination, Alloc,
              decltype(get<Indices>(declval<Source>()))>...> {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices, bool SameArity>
struct tuple_allocator_source_constructible_if_same : false_type {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices>
struct tuple_allocator_source_constructible_if_same<
    Alloc, Destination, Source, Indices, true>
    : tuple_allocator_source_constructible<
          Alloc, Destination, Source, Indices> {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices, bool SameArity>
struct tuple_allocator_source_nothrow_constructible_if_same : false_type {};

template<typename Alloc, typename Destination, typename Source,
         typename Indices>
struct tuple_allocator_source_nothrow_constructible_if_same<
    Alloc, Destination, Source, Indices, true>
    : tuple_allocator_source_nothrow_constructible<
          Alloc, Destination, Source, Indices> {};

/* A const-reference tuple element is a reference to a potentially writable
 * object, not a rebindable pointer.  Keep the pointee assignment capability
 * in the holder's type so C++11 traits and tuple special members agree with
 * the operation that will actually be performed. */
template<typename T, bool CopyAssignable =
                         is_assignable<const T&, const T&>::value,
         bool MoveAssignable =
                         is_assignable<const T&, const T&&>::value>
class tuple_const_ref_holder;

template<typename T>
class tuple_const_ref_holder<T, false, false> {
    const T* pointer_;
public:
    constexpr explicit tuple_const_ref_holder(const T& value)
        : pointer_(&value) {}
    tuple_const_ref_holder(const tuple_const_ref_holder&) = default;
    tuple_const_ref_holder(tuple_const_ref_holder&&) = default;
    tuple_const_ref_holder& operator=(const tuple_const_ref_holder&) = delete;
    tuple_const_ref_holder& operator=(tuple_const_ref_holder&&) = delete;
    constexpr const T& get() const noexcept { return *pointer_; }
};

template<typename T>
class tuple_const_ref_holder<T, true, false> {
    const T* pointer_;
public:
    constexpr explicit tuple_const_ref_holder(const T& value)
        : pointer_(&value) {}
    tuple_const_ref_holder(const tuple_const_ref_holder&) = default;
    tuple_const_ref_holder(tuple_const_ref_holder&&) = default;
    tuple_const_ref_holder& operator=(const tuple_const_ref_holder& other)
        noexcept(is_nothrow_assignable<const T&, const T&>::value) {
        *pointer_ = *other.pointer_;
        return *this;
    }
    tuple_const_ref_holder& operator=(tuple_const_ref_holder&&) = delete;
    constexpr const T& get() const noexcept { return *pointer_; }
};

template<typename T>
class tuple_const_ref_holder<T, false, true> {
    const T* pointer_;
public:
    constexpr explicit tuple_const_ref_holder(const T& value)
        : pointer_(&value) {}
    tuple_const_ref_holder(const tuple_const_ref_holder&) = default;
    tuple_const_ref_holder(tuple_const_ref_holder&&) = default;
    tuple_const_ref_holder& operator=(const tuple_const_ref_holder&) = delete;
    tuple_const_ref_holder& operator=(tuple_const_ref_holder&& other)
        noexcept(is_nothrow_assignable<const T&, const T&&>::value) {
        *pointer_ = static_cast<const T&&>(*other.pointer_);
        return *this;
    }
    constexpr const T& get() const noexcept { return *pointer_; }
};

template<typename T>
class tuple_const_ref_holder<T, true, true> {
    const T* pointer_;
public:
    constexpr explicit tuple_const_ref_holder(const T& value)
        : pointer_(&value) {}
    tuple_const_ref_holder(const tuple_const_ref_holder&) = default;
    tuple_const_ref_holder(tuple_const_ref_holder&&) = default;
    tuple_const_ref_holder& operator=(const tuple_const_ref_holder& other)
        noexcept(is_nothrow_assignable<const T&, const T&>::value) {
        *pointer_ = *other.pointer_;
        return *this;
    }
    tuple_const_ref_holder& operator=(tuple_const_ref_holder&& other)
        noexcept(is_nothrow_assignable<const T&, const T&&>::value) {
        *pointer_ = static_cast<const T&&>(*other.pointer_);
        return *this;
    }
    constexpr const T& get() const noexcept { return *pointer_; }
};

} /* namespace detail */

/* index_sequence, make_index_sequence は utility.h で定義済み */

/* ═══════════════════════════════════════════════════════════════
 * tuple実装 - インデックス付き葉ノード方式
 * ═══════════════════════════════════════════════════════════════*/

/* 葉ノード - インデックスで一意に識別 */
template<size_t I, typename T>
class tuple_leaf {
public:
    T value_;  /* 構造化束縛をサポートするためpublic */
    template<typename U = T,
             enable_if_t<is_default_constructible<U>::value, int> = 0>
    constexpr tuple_leaf() noexcept(is_nothrow_default_constructible<U>::value)
        : value_() {}

    /* Only accept types convertible to T, not tuples */
    template<typename U,
             typename = typename enable_if<
                 is_constructible<T, U&&>::value &&
                 !is_same<typename decay<U>::type, tuple_leaf>::value
             >::type>
    constexpr tuple_leaf(U&& val)
        noexcept(is_nothrow_constructible<T, U&&>::value)
        : value_(std::forward<U>(val)) {}

    template<typename Alloc, typename... Args,
             enable_if_t<!uses_allocator<T, Alloc>::value &&
                             is_constructible<T, Args&&...>::value,
                         int> = 0>
    constexpr tuple_leaf(allocator_arg_t, const Alloc&, Args&&... args)
        noexcept(is_nothrow_constructible<T, Args&&...>::value)
        : value_(std::forward<Args>(args)...) {}

    template<typename Alloc, typename... Args,
             enable_if_t<uses_allocator<T, Alloc>::value &&
                             is_constructible<T, allocator_arg_t,
                                              const Alloc&, Args&&...>::value,
                         int> = 0>
    constexpr tuple_leaf(allocator_arg_t, const Alloc& alloc,
                         Args&&... args)
        noexcept(is_nothrow_constructible<T, allocator_arg_t, const Alloc&,
                                          Args&&...>::value)
        : value_(allocator_arg, alloc, std::forward<Args>(args)...) {}

    template<typename Alloc, typename... Args,
             enable_if_t<uses_allocator<T, Alloc>::value &&
                             !is_constructible<T, allocator_arg_t,
                                               const Alloc&, Args&&...>::value &&
                             is_constructible<T, Args&&..., const Alloc&>::value,
                         int> = 0>
    constexpr tuple_leaf(allocator_arg_t, const Alloc& alloc,
                         Args&&... args)
        noexcept(is_nothrow_constructible<T, Args&&..., const Alloc&>::value)
        : value_(std::forward<Args>(args)..., alloc) {}

    tuple_leaf(const tuple_leaf&) = default;
    tuple_leaf(tuple_leaf&&) = default;
    tuple_leaf& operator=(const tuple_leaf&) = default;
    tuple_leaf& operator=(tuple_leaf&&) = default;

    RIN_TUPLE_CONSTEXPR14 T& value() noexcept { return value_; }
    constexpr const T& value() const noexcept { return value_; }
};

/* 葉ノード特殊化 - 参照型 (lvalue reference) */
template<size_t I, typename T>
class tuple_leaf<I, T&> {
public:
    T& value_;  /* 構造化束縛をサポートするためpublic */
    constexpr tuple_leaf(T& val) : value_(val) {}

    template<typename Alloc>
    constexpr tuple_leaf(allocator_arg_t, const Alloc&, T& val)
        noexcept
        : value_(val) {}

    tuple_leaf(const tuple_leaf& other) = default;
    tuple_leaf(tuple_leaf&& other) = default;

    /* 参照先に代入 (参照自体は再バインドしない) */
    tuple_leaf& operator=(const tuple_leaf& other) {
        value_ = other.value_;
        return *this;
    }

    tuple_leaf& operator=(tuple_leaf&& other) {
        value_ = std::move(other.value_);
        return *this;
    }

    /* 異なる型からの代入 */
    template<typename U>
    tuple_leaf& operator=(U&& val) {
        value_ = std::forward<U>(val);
        return *this;
    }

    RIN_TUPLE_CONSTEXPR14 T& value() noexcept { return value_; }
    constexpr T& value() const noexcept { return value_; }  /* 参照は再バインド不可 */
};

/* 葉ノード特殊化 - const参照型 */
template<size_t I, typename T>
class tuple_leaf<I, const T&> {
public:
    detail::tuple_const_ref_holder<T> value_;  /* 構造化束縛をサポートするためpublic */
    constexpr tuple_leaf(const T& val) : value_(val) {}

    template<typename Alloc>
    constexpr tuple_leaf(allocator_arg_t, const Alloc&, const T& val)
        : value_(val) {}

    tuple_leaf(const tuple_leaf& other) = default;
    tuple_leaf(tuple_leaf&& other) = default;
    tuple_leaf& operator=(const tuple_leaf& other) = default;
    tuple_leaf& operator=(tuple_leaf&& other) = default;

    template<typename U,
             enable_if_t<is_assignable<const T&, U&&>::value, int> = 0>
    RIN_TUPLE_CONSTEXPR14 tuple_leaf& operator=(U&& other)
        noexcept(is_nothrow_assignable<const T&, U&&>::value) {
        value_.get() = std::forward<U>(other);
        return *this;
    }

    constexpr const T& value() const noexcept { return value_.get(); }
};

/* tuple実装 - インデックスを追跡 */
template<size_t I, typename... Types>
class tuple_impl;

/* 空の終端 */
template<size_t I>
class tuple_impl<I> {
public:
    constexpr tuple_impl() = default;

    template<typename Alloc>
    constexpr tuple_impl(allocator_arg_t, const Alloc&) {}

    template<typename Alloc>
    constexpr tuple_impl(allocator_arg_t, const Alloc&, const tuple_impl&) {}

    template<typename Alloc>
    constexpr tuple_impl(allocator_arg_t, const Alloc&, tuple_impl&&) {}

    RIN_TUPLE_CONSTEXPR14 void swap(tuple_impl&) noexcept {}
};

/* 再帰的な実装 - インデックスを増加 */
template<size_t I, typename Head, typename... Tail>
class tuple_impl<I, Head, Tail...> : public tuple_leaf<I, Head>, public tuple_impl<I + 1, Tail...> {
    using head_base = tuple_leaf<I, Head>;
    using tail_base = tuple_impl<I + 1, Tail...>;
    
public:
    template<typename Dummy = void,
             enable_if_t<conjunction<
                 is_same<Dummy, Dummy>,
                 is_default_constructible<Head>,
                 is_default_constructible<Tail>...>::value, int> = 0>
    constexpr tuple_impl()
        noexcept(is_nothrow_default_constructible<Head>::value &&
                 conjunction<is_nothrow_default_constructible<Tail>...>::value)
        : head_base(), tail_base() {}

    template<typename H, typename... T,
             typename = enable_if_t<
                 sizeof...(T) == sizeof...(Tail) &&
                 is_constructible<Head, H&&>::value &&
                 conjunction<is_constructible<Tail, T&&>...>::value
             >>
    constexpr tuple_impl(H&& head, T&&... tail)
        noexcept(is_nothrow_constructible<Head, H&&>::value &&
                 conjunction<is_nothrow_constructible<Tail, T&&>...>::value)
        : head_base(std::forward<H>(head)), tail_base(std::forward<T>(tail)...) {}

    template<typename Source, size_t... Indices>
    constexpr tuple_impl(
        Source&& source, index_sequence<Indices...>,
        enable_if_t<detail::tuple_source_constructible<
                        tuple<Head, Tail...>, Source&&,
                        index_sequence<Indices...>>::value,
                    int>* = nullptr)
        noexcept(detail::tuple_source_nothrow_constructible<
                     tuple<Head, Tail...>, Source&&,
                     index_sequence<Indices...>>::value)
        : tuple_impl(get<Indices>(std::forward<Source>(source))...) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<Head, Alloc>::value &&
                             conjunction<detail::tuple_uses_allocator_constructible<
                                 Tail, Alloc>...>::value,
                         int> = 0>
    constexpr tuple_impl(allocator_arg_t, const Alloc& alloc)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     Head, Alloc>::value &&
                 conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Tail, Alloc>...>::value)
        : head_base(allocator_arg, alloc), tail_base(allocator_arg, alloc) {}

    template<typename Alloc, typename H, typename... T,
             enable_if_t<sizeof...(T) == sizeof...(Tail) &&
                             detail::tuple_uses_allocator_constructible<
                                 Head, Alloc, H>::value &&
                             conjunction<detail::tuple_uses_allocator_constructible<
                                 Tail, Alloc, T>...>::value,
                         int> = 0>
    constexpr tuple_impl(allocator_arg_t, const Alloc& alloc, H&& head,
                         T&&... tail)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     Head, Alloc, H>::value &&
                 conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Tail, Alloc, T>...>::value)
        : head_base(allocator_arg, alloc, std::forward<H>(head)),
          tail_base(allocator_arg, alloc, std::forward<T>(tail)...) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             Head, Alloc, const Head&>::value &&
                             conjunction<detail::tuple_uses_allocator_constructible<
                                 Tail, Alloc, const Tail&>...>::value,
                         int> = 0>
    constexpr tuple_impl(allocator_arg_t, const Alloc& alloc,
                         const tuple_impl& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     Head, Alloc, const Head&>::value &&
                 conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Tail, Alloc, const Tail&>...>::value)
        : head_base(allocator_arg, alloc, other.head()),
          tail_base(allocator_arg, alloc, other.tail()) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             Head, Alloc, Head>::value &&
                             conjunction<detail::tuple_uses_allocator_constructible<
                                 Tail, Alloc, Tail>...>::value,
                         int> = 0>
    constexpr tuple_impl(allocator_arg_t, const Alloc& alloc,
                         tuple_impl&& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     Head, Alloc, Head>::value &&
                 conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Tail, Alloc, Tail>...>::value)
        : head_base(allocator_arg, alloc, std::move(other.head())),
          tail_base(allocator_arg, alloc, std::move(other.tail())) {}

    template<typename Alloc, typename Source, size_t... Indices>
    constexpr tuple_impl(
        allocator_arg_t, const Alloc& alloc, Source&& source,
        index_sequence<Indices...>,
        enable_if_t<detail::tuple_allocator_source_constructible<
                        Alloc, tuple<Head, Tail...>, Source&&,
                        index_sequence<Indices...>>::value,
                     int>* = nullptr)
        noexcept(detail::tuple_allocator_source_nothrow_constructible<
                     Alloc, tuple<Head, Tail...>, Source&&,
                     index_sequence<Indices...>>::value)
        : tuple_impl(allocator_arg, alloc,
                     get<Indices>(std::forward<Source>(source))...) {}

    tuple_impl(const tuple_impl&) = default;
    tuple_impl(tuple_impl&&) = default;
    tuple_impl& operator=(const tuple_impl&) = default;
    tuple_impl& operator=(tuple_impl&&) = default;
    
    Head& head() noexcept { return head_base::value(); }
    const Head& head() const noexcept { return head_base::value(); }
    
    tail_base& tail() noexcept { return *this; }
    const tail_base& tail() const noexcept { return *this; }
    
    RIN_TUPLE_CONSTEXPR14 void swap(tuple_impl& other)
        noexcept(is_nothrow_swappable<Head>::value &&
                 conjunction<is_nothrow_swappable<Tail>...>::value) {
        using std::swap;
        swap(head(), other.head());
        tail().swap(other.tail());
    }
};

/* Helper to assign between tuple types */
namespace detail {
    template<size_t I, size_t N>
    struct tuple_assign_impl {
        template<typename Dst, typename Src>
        static RIN_TUPLE_CONSTEXPR14 void assign(Dst& dst, const Src& src) {
            get<I>(dst) = get<I>(src);
            tuple_assign_impl<I + 1, N>::assign(dst, src);
        }
        template<typename Dst, typename Src>
        static RIN_TUPLE_CONSTEXPR14 void move_assign(Dst& dst, Src&& src) {
            get<I>(dst) = std::move(get<I>(src));
            tuple_assign_impl<I + 1, N>::move_assign(dst, std::move(src));
        }
    };

    template<size_t N>
    struct tuple_assign_impl<N, N> {
        template<typename Dst, typename Src>
        static RIN_TUPLE_CONSTEXPR14 void assign(Dst&, const Src&) {}
        template<typename Dst, typename Src>
        static RIN_TUPLE_CONSTEXPR14 void move_assign(Dst&, Src&&) {}
    };
}

/* Forward declaration of pair */
template<typename T1, typename T2> struct pair;

/* 公開tuple型 */
template<typename... Types>
class tuple : public tuple_impl<0, Types...> {
    using base = tuple_impl<0, Types...>;
public:
    template<typename Dummy = void,
             enable_if_t<conjunction<
                 is_same<Dummy, Dummy>,
                 is_default_constructible<Types>...>::value, int> = 0>
    constexpr tuple()
        noexcept(conjunction<is_nothrow_default_constructible<Types>...>::value)
        : base() {}

    template<typename... Args,
             enable_if_t<sizeof...(Args) != 0 &&
                             detail::tuple_pack_constructible<
                                 tuple<Types...>, tuple<Args&&...>>::value &&
                             detail::tuple_pack_convertible<
                                 tuple<Types...>, tuple<Args&&...>>::value,
                         int> = 0>
    constexpr tuple(Args&&... args)
        noexcept(conjunction<is_nothrow_constructible<Types, Args&&>...>::value)
        : base(std::forward<Args>(args)...) {}

    template<typename... Args,
             enable_if_t<sizeof...(Args) != 0 &&
                             detail::tuple_pack_constructible<
                                 tuple<Types...>, tuple<Args&&...>>::value &&
                             !detail::tuple_pack_convertible<
                                 tuple<Types...>, tuple<Args&&...>>::value,
                         int> = 0>
    explicit constexpr tuple(Args&&... args)
        noexcept(conjunction<is_nothrow_constructible<Types, Args&&>...>::value)
        : base(std::forward<Args>(args)...) {}

    template<typename... UTypes,
             enable_if_t<!is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    constexpr tuple(const tuple<UTypes...>& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, const tuple<UTypes...>&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(source, make_index_sequence<sizeof...(Types)>{}) {}

    template<typename... UTypes,
             enable_if_t<!is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    explicit constexpr tuple(const tuple<UTypes...>& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, const tuple<UTypes...>&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(source, make_index_sequence<sizeof...(Types)>{}) {}

    template<typename... UTypes,
             enable_if_t<!is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    constexpr tuple(tuple<UTypes...>&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, tuple<UTypes...>&&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(std::move(source), make_index_sequence<sizeof...(Types)>{}) {}

    template<typename... UTypes,
             enable_if_t<!is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    explicit constexpr tuple(tuple<UTypes...>&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, tuple<UTypes...>&&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(std::move(source), make_index_sequence<sizeof...(Types)>{}) {}

#if __cplusplus > 202002L
    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, sizeof...(Types)>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value,
                         int> = 0>
    constexpr tuple(Source&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source, sizeof...(Types)>::value>::value)
        : base(std::forward<Source>(source),
               make_index_sequence<sizeof...(Types)>{}) {}

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, sizeof...(Types)>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value,
                         int> = 0>
    explicit constexpr tuple(Source&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source, sizeof...(Types)>::value>::value)
        : base(std::forward<Source>(source),
               make_index_sequence<sizeof...(Types)>{}) {}

    /* C++23 tuple-like sources participate in allocator-extended
     * construction with the same element-wise leading/trailing allocator
     * dispatch as tuple and pair sources.  Keep the source constrained to
     * the additional tuple-like set so ordinary tuple arities cannot reach
     * get<I> outside their bounds. */
    template<typename Alloc, typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, sizeof...(Types)>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, Source&& source)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source, sizeof...(Types)>::value>::value)
        : base(allocator_arg, alloc, std::forward<Source>(source),
               make_index_sequence<sizeof...(Types)>{}) {}

    template<typename Alloc, typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, sizeof...(Types)>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source, sizeof...(Types)>::value>::value,
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             Source&& source)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source, sizeof...(Types)>::value>::value)
        : base(allocator_arg, alloc, std::forward<Source>(source),
               make_index_sequence<sizeof...(Types)>{}) {}
#endif

    template<typename Alloc,
             enable_if_t<conjunction<detail::tuple_uses_allocator_constructible<
                             Types, Alloc>...>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc)
        noexcept(conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Types, Alloc>...>::value)
        : base(allocator_arg, alloc) {}

    template<typename Alloc, typename... Args,
             enable_if_t<sizeof...(Args) == sizeof...(Types) &&
                             sizeof...(Args) != 0 &&
                             conjunction<detail::tuple_uses_allocator_constructible<
                                 Types, Alloc, Args>...>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, Args&&... args)
        noexcept(conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Types, Alloc, Args>...>::value)
        : base(allocator_arg, alloc, std::forward<Args>(args)...) {}

    template<typename Alloc,
             enable_if_t<conjunction<detail::tuple_uses_allocator_constructible<
                             Types, Alloc, const Types&>...>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, const tuple& other)
        noexcept(conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Types, Alloc, const Types&>...>::value)
        : base(allocator_arg, alloc, static_cast<const base&>(other)) {}

    template<typename Alloc,
             enable_if_t<conjunction<detail::tuple_uses_allocator_constructible<
                             Types, Alloc, Types>...>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, tuple&& other)
        noexcept(conjunction<detail::tuple_uses_allocator_nothrow_constructible<
                     Types, Alloc, Types>...>::value)
        : base(allocator_arg, alloc, static_cast<base&&>(other)) {}

    template<typename Alloc, typename... UTypes,
             enable_if_t<sizeof...(UTypes) == sizeof...(Types) &&
                             !is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>,
                                 const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc,
                    const tuple<UTypes...>& other)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, const tuple<UTypes...>&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(allocator_arg, alloc, other,
               make_index_sequence<sizeof...(Types)>{}) {}

    template<typename Alloc, typename... UTypes,
             enable_if_t<sizeof...(UTypes) == sizeof...(Types) &&
                             !is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>,
                                 const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, const tuple<UTypes...>&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             const tuple<UTypes...>& other)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, const tuple<UTypes...>&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(allocator_arg, alloc, other,
               make_index_sequence<sizeof...(Types)>{}) {}

    template<typename Alloc, typename... UTypes,
             enable_if_t<sizeof...(UTypes) == sizeof...(Types) &&
                             !is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc,
                    tuple<UTypes...>&& other)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, tuple<UTypes...>&&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(allocator_arg, alloc, std::move(other),
               make_index_sequence<sizeof...(Types)>{}) {}

    template<typename Alloc, typename... UTypes,
             enable_if_t<sizeof...(UTypes) == sizeof...(Types) &&
                             !is_same<tuple<Types...>, tuple<UTypes...>>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<Types...>, tuple<UTypes...>&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 sizeof...(Types) == sizeof...(UTypes)>::value,
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             tuple<UTypes...>&& other)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<Types...>, tuple<UTypes...>&&,
                     make_index_sequence<sizeof...(Types)>,
                     sizeof...(Types) == sizeof...(UTypes)>::value)
        : base(allocator_arg, alloc, std::move(other),
               make_index_sequence<sizeof...(Types)>{}) {}

    tuple(const tuple&) = default;
    tuple(tuple&&) = default;
    tuple& operator=(const tuple&) = default;
    tuple& operator=(tuple&&) = default;

#if __cplusplus > 202002L
    /* C++23 permits assignment through a const tuple when every stored
     * element is itself writable through const.  That is normally a tuple of
     * references; ordinary value elements deliberately do not participate. */
    constexpr const tuple& operator=(const tuple& other) const
        noexcept(conjunction<is_nothrow_assignable<const Types&,
                                                   const Types&>...>::value)
        requires (conjunction<is_copy_assignable<const Types>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::assign(*this, other);
        return *this;
    }

    constexpr const tuple& operator=(tuple&& other) const
        noexcept(conjunction<is_nothrow_assignable<const Types&,
                                                   Types&&>...>::value)
        requires (conjunction<is_assignable<const Types&, Types>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::move_assign(
            *this, std::move(other));
        return *this;
    }
#endif

    /* Assignment from different tuple type (for std::tie support) */
    template<typename... UTypes,
             enable_if_t<
                 sizeof...(UTypes) == sizeof...(Types) &&
                 conjunction<is_assignable<Types&, const UTypes&>...>::value,
                 int> = 0>
    tuple& operator=(const tuple<UTypes...>& other)
        noexcept(conjunction<is_nothrow_assignable<
                    Types&, const UTypes&>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::assign(*this, other);
        return *this;
    }

    template<typename... UTypes,
             enable_if_t<
                 sizeof...(UTypes) == sizeof...(Types) &&
                 conjunction<is_assignable<Types&, UTypes&&>...>::value,
                 int> = 0>
    tuple& operator=(tuple<UTypes...>&& other)
        noexcept(conjunction<is_nothrow_assignable<
                    Types&, UTypes&&>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::move_assign(*this, std::move(other));
        return *this;
    }

#if __cplusplus > 202002L
    template<typename... UTypes>
        requires (sizeof...(UTypes) == sizeof...(Types)) &&
                 conjunction<is_assignable<const Types&, const UTypes&>...>::value
    constexpr const tuple& operator=(const tuple<UTypes...>& other) const
        noexcept(conjunction<is_nothrow_assignable<
                    const Types&, const UTypes&>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::assign(*this, other);
        return *this;
    }

    template<typename... UTypes>
        requires (sizeof...(UTypes) == sizeof...(Types)) &&
                 conjunction<is_assignable<const Types&, UTypes>...>::value
    constexpr const tuple& operator=(tuple<UTypes...>&& other) const
        noexcept(conjunction<is_nothrow_assignable<
                    const Types&, UTypes&&>...>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::move_assign(
            *this, std::move(other));
        return *this;
    }

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 const Source&, sizeof...(Types)>::value &&
                             detail::tuple_source_assignable_if_enabled<
                                 tuple<Types...>, const Source&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     const Source&, sizeof...(Types)>::value>::value,
                         int> = 0>
    constexpr tuple& operator=(const Source& source)
        noexcept(detail::tuple_source_nothrow_assignable_if_enabled<
                     tuple<Types...>, const Source&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         const Source&, sizeof...(Types)>::value>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::assign(*this, source);
        return *this;
    }

    template<typename Source,
             enable_if_t<!is_lvalue_reference<Source>::value &&
                             detail::is_additional_tuple_like_source<
                                 Source&&, sizeof...(Types)>::value &&
                             detail::tuple_source_assignable_if_enabled<
                                 tuple<Types...>, Source&&,
                                 make_index_sequence<sizeof...(Types)>,
                                 detail::is_additional_tuple_like_source<
                                     Source&&, sizeof...(Types)>::value>::value,
                         int> = 0>
    constexpr tuple& operator=(Source&& source)
        noexcept(detail::tuple_source_nothrow_assignable_if_enabled<
                     tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source&&, sizeof...(Types)>::value>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::move_assign(
            *this, std::forward<Source>(source));
        return *this;
    }

    template<typename Source>
        requires detail::is_additional_tuple_like_source<
                     const Source&, sizeof...(Types)>::value &&
                 detail::tuple_source_const_assignable_if_enabled<
                     tuple<Types...>, const Source&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         const Source&, sizeof...(Types)>::value>::value
    constexpr const tuple& operator=(const Source& source) const
        noexcept(detail::tuple_source_const_nothrow_assignable_if_enabled<
                     tuple<Types...>, const Source&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         const Source&, sizeof...(Types)>::value>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::assign(*this, source);
        return *this;
    }

    template<typename Source>
        requires (!is_lvalue_reference<Source>::value) &&
                 detail::is_additional_tuple_like_source<
                     Source&&, sizeof...(Types)>::value &&
                 detail::tuple_source_const_assignable_if_enabled<
                     tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source&&, sizeof...(Types)>::value>::value
    constexpr const tuple& operator=(Source&& source) const
        noexcept(detail::tuple_source_const_nothrow_assignable_if_enabled<
                     tuple<Types...>, Source&&,
                     make_index_sequence<sizeof...(Types)>,
                     detail::is_additional_tuple_like_source<
                         Source&&, sizeof...(Types)>::value>::value) {
        detail::tuple_assign_impl<0, sizeof...(Types)>::move_assign(
            *this, std::forward<Source>(source));
        return *this;
    }
#endif

    RIN_TUPLE_CONSTEXPR14 void swap(tuple& other)
        noexcept(conjunction<is_nothrow_swappable<Types>...>::value) {
        base::swap(other);
    }
};

/* Specialization for 2-element tuple to support pair interoperability */
template<typename T1, typename T2>
class tuple<T1, T2> : public tuple_impl<0, T1, T2> {
    using base = tuple_impl<0, T1, T2>;
    using leaf0 = tuple_leaf<0, T1>;
    using leaf1 = tuple_leaf<1, T2>;

    RIN_TUPLE_CONSTEXPR14 T1& first() noexcept {
        return static_cast<leaf0&>(*this).value();
    }
    constexpr const T1& first() const noexcept {
        return static_cast<const leaf0&>(*this).value();
    }
    RIN_TUPLE_CONSTEXPR14 T2& second() noexcept {
        return static_cast<leaf1&>(*this).value();
    }
    constexpr const T2& second() const noexcept {
        return static_cast<const leaf1&>(*this).value();
    }

public:
    template<typename Dummy = void,
             enable_if_t<conjunction<
                 is_same<Dummy, Dummy>,
                 is_default_constructible<T1>,
                 is_default_constructible<T2>>::value, int> = 0>
    constexpr tuple()
        noexcept(is_nothrow_default_constructible<T1>::value &&
                 is_nothrow_default_constructible<T2>::value)
        : base() {}

    template<typename U1, typename U2,
             enable_if_t<
                 !detail::is_tuple_specialization<decay_t<U1>>::value &&
                 is_constructible<T1, U1&&>::value &&
                 is_constructible<T2, U2&&>::value &&
                 is_convertible<U1&&, T1>::value &&
                 is_convertible<U2&&, T2>::value,
                 int> = 0>
    constexpr tuple(U1&& a, U2&& b)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::forward<U1>(a), std::forward<U2>(b)) {}

    template<typename U1, typename U2,
             enable_if_t<
                 !detail::is_tuple_specialization<decay_t<U1>>::value &&
                 is_constructible<T1, U1&&>::value &&
                 is_constructible<T2, U2&&>::value &&
                 !(is_convertible<U1&&, T1>::value &&
                   is_convertible<U2&&, T2>::value),
                 int> = 0>
    explicit constexpr tuple(U1&& a, U2&& b)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::forward<U1>(a), std::forward<U2>(b)) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<T1, Alloc>::value &&
                             detail::tuple_uses_allocator_constructible<T2, Alloc>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc>::value)
        : base(allocator_arg, alloc) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, U1>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, U2>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, U1&& a, U2&& b)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, U1>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, U2>::value)
        : base(allocator_arg, alloc, std::forward<U1>(a),
               std::forward<U2>(b)) {}

    /* pair construction follows the same conditional-explicit rule as
     * direct values and uses the indexed source owner to preserve cvref. */
    template<typename U1, typename U2,
             enable_if_t<
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, const pair<U1, U2>&,
                     index_sequence<0, 1>>::value &&
                 detail::tuple_source_convertible<
                     tuple<T1, T2>, const pair<U1, U2>&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    constexpr tuple(const pair<U1, U2>& p)
        noexcept(is_nothrow_constructible<T1, const U1&>::value &&
                 is_nothrow_constructible<T2, const U2&>::value)
        : base(p, index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, const pair<U1, U2>&,
                     index_sequence<0, 1>>::value &&
                 !detail::tuple_source_convertible<
                     tuple<T1, T2>, const pair<U1, U2>&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    explicit constexpr tuple(const pair<U1, U2>& p)
        noexcept(is_nothrow_constructible<T1, const U1&>::value &&
                 is_nothrow_constructible<T2, const U2&>::value)
        : base(p, index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, pair<U1, U2>&&,
                     index_sequence<0, 1>>::value &&
                 detail::tuple_source_convertible<
                     tuple<T1, T2>, pair<U1, U2>&&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    constexpr tuple(pair<U1, U2>&& p)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::move(p), index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, pair<U1, U2>&&,
                     index_sequence<0, 1>>::value &&
                 !detail::tuple_source_convertible<
                     tuple<T1, T2>, pair<U1, U2>&&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    explicit constexpr tuple(pair<U1, U2>&& p)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::move(p), index_sequence<0, 1>{}) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, const U1&>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, const U2&>::value &&
                             is_convertible<const U1&, T1>::value &&
                             is_convertible<const U2&, T2>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc,
                    const pair<U1, U2>& p)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, const U1&>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, const U2&>::value)
        : base(allocator_arg, alloc, p.first, p.second) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, const U1&>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, const U2&>::value &&
                             (!is_convertible<const U1&, T1>::value ||
                              !is_convertible<const U2&, T2>::value),
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             const pair<U1, U2>& p)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, const U1&>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, const U2&>::value)
        : base(allocator_arg, alloc, p.first, p.second) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, U1>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, U2>::value &&
                             is_convertible<U1, T1>::value &&
                             is_convertible<U2, T2>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, pair<U1, U2>&& p)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, U1>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, U2>::value)
        : base(allocator_arg, alloc, std::forward<U1>(p.first),
               std::forward<U2>(p.second)) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, U1>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, U2>::value &&
                             (!is_convertible<U1, T1>::value ||
                              !is_convertible<U2, T2>::value),
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             pair<U1, U2>&& p)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, U1>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, U2>::value)
        : base(allocator_arg, alloc, std::forward<U1>(p.first),
               std::forward<U2>(p.second)) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<
                             T1, Alloc, const T1&>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, const T2&>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, const tuple& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, const T1&>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, const T2&>::value)
        : base(allocator_arg, alloc, static_cast<const base&>(other)) {}

    template<typename Alloc,
             enable_if_t<detail::tuple_uses_allocator_constructible<T1, Alloc, T1>::value &&
                             detail::tuple_uses_allocator_constructible<T2, Alloc, T2>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, tuple&& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, T1>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, T2>::value)
        : base(allocator_arg, alloc, static_cast<base&&>(other)) {}

    tuple(const tuple&) = default;
    tuple(tuple&&) = default;
    tuple& operator=(const tuple&) = default;
    tuple& operator=(tuple&&) = default;

#if __cplusplus > 202002L
    constexpr const tuple& operator=(const tuple& other) const
        noexcept(is_nothrow_assignable<const T1&, const T1&>::value &&
                 is_nothrow_assignable<const T2&, const T2&>::value)
        requires (is_copy_assignable<const T1>::value &&
                  is_copy_assignable<const T2>::value) {
        first() = other.first();
        second() = other.second();
        return *this;
    }

    constexpr const tuple& operator=(tuple&& other) const
        noexcept(is_nothrow_assignable<const T1&, T1&&>::value &&
                 is_nothrow_assignable<const T2&, T2&&>::value)
        requires (is_assignable<const T1&, T1>::value &&
                  is_assignable<const T2&, T2>::value) {
        first() = std::move(other.first());
        second() = std::move(other.second());
        return *this;
    }
#endif

    /* Converting tuple construction is available for every element pair and
     * stays explicit whenever one element conversion is explicit. */
    template<typename U1, typename U2,
             enable_if_t<
                 !(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, const tuple<U1, U2>&,
                     index_sequence<0, 1>>::value &&
                 detail::tuple_source_convertible<
                     tuple<T1, T2>, const tuple<U1, U2>&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    constexpr tuple(const tuple<U1, U2>& other)
        noexcept(is_nothrow_constructible<T1, const U1&>::value &&
                 is_nothrow_constructible<T2, const U2&>::value)
        : base(other, index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 !(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, const tuple<U1, U2>&,
                     index_sequence<0, 1>>::value &&
                 !detail::tuple_source_convertible<
                     tuple<T1, T2>, const tuple<U1, U2>&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    explicit constexpr tuple(const tuple<U1, U2>& other)
        noexcept(is_nothrow_constructible<T1, const U1&>::value &&
                 is_nothrow_constructible<T2, const U2&>::value)
        : base(other, index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 !(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, tuple<U1, U2>&&,
                     index_sequence<0, 1>>::value &&
                 detail::tuple_source_convertible<
                     tuple<T1, T2>, tuple<U1, U2>&&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    constexpr tuple(tuple<U1, U2>&& other)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::move(other), index_sequence<0, 1>{}) {}

    template<typename U1, typename U2,
             enable_if_t<
                 !(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                 detail::tuple_source_constructible<
                     tuple<T1, T2>, tuple<U1, U2>&&,
                     index_sequence<0, 1>>::value &&
                 !detail::tuple_source_convertible<
                     tuple<T1, T2>, tuple<U1, U2>&&,
                     index_sequence<0, 1>>::value,
                 int> = 0>
    explicit constexpr tuple(tuple<U1, U2>&& other)
        noexcept(is_nothrow_constructible<T1, U1&&>::value &&
                 is_nothrow_constructible<T2, U2&&>::value)
        : base(std::move(other), index_sequence<0, 1>{}) {}

#if __cplusplus > 202002L
    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, 2>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value,
                         int> = 0>
    constexpr tuple(Source&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source, 2>::value>::value)
        : base(std::forward<Source>(source), index_sequence<0, 1>{}) {}

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, 2>::value &&
                             detail::tuple_source_constructible_if_same<
                                 tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value,
                         int> = 0>
    explicit constexpr tuple(Source&& source)
        noexcept(detail::tuple_source_nothrow_constructible_if_same<
                     tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source, 2>::value>::value)
        : base(std::forward<Source>(source), index_sequence<0, 1>{}) {}

    template<typename Alloc, typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, 2>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value &&
                             detail::tuple_source_convertible_if_same<
                                 tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc, Source&& source)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source, 2>::value>::value)
        : base(allocator_arg, alloc, std::forward<Source>(source),
               index_sequence<0, 1>{}) {}

    template<typename Alloc, typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, 2>::value &&
                             detail::tuple_allocator_source_constructible_if_same<
                                 Alloc, tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value &&
                             !detail::tuple_source_convertible_if_same<
                                 tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source, 2>::value>::value,
                         int> = 0>
    explicit constexpr tuple(allocator_arg_t, const Alloc& alloc,
                             Source&& source)
        noexcept(detail::tuple_allocator_source_nothrow_constructible_if_same<
                     Alloc, tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source, 2>::value>::value)
        : base(allocator_arg, alloc, std::forward<Source>(source),
               index_sequence<0, 1>{}) {}
#endif

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<!(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                             detail::tuple_uses_allocator_constructible<
                                 T1, Alloc, const U1&>::value &&
                             detail::tuple_uses_allocator_constructible<
                                 T2, Alloc, const U2&>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc,
                    const tuple<U1, U2>& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, const U1&>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, const U2&>::value)
        : base(allocator_arg, alloc,
               static_cast<const tuple_leaf<0, U1>&>(other).value(),
               static_cast<const tuple_leaf<1, U2>&>(other).value()) {}

    template<typename Alloc, typename U1, typename U2,
             enable_if_t<!(is_same<T1, U1>::value && is_same<T2, U2>::value) &&
                             detail::tuple_uses_allocator_constructible<T1, Alloc, U1>::value &&
                             detail::tuple_uses_allocator_constructible<T2, Alloc, U2>::value,
                         int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc& alloc,
                    tuple<U1, U2>&& other)
        noexcept(detail::tuple_uses_allocator_nothrow_constructible<
                     T1, Alloc, U1>::value &&
                 detail::tuple_uses_allocator_nothrow_constructible<
                     T2, Alloc, U2>::value)
        : base(allocator_arg, alloc,
               std::forward<U1>(static_cast<tuple_leaf<0, U1>&>(other).value()),
               std::forward<U2>(static_cast<tuple_leaf<1, U2>&>(other).value())) {}

    /* Assignment from different tuple type */
    template<typename U1, typename U2,
             enable_if_t<
                 is_assignable<T1&, const U1&>::value &&
                 is_assignable<T2&, const U2&>::value,
                 int> = 0>
    tuple& operator=(const tuple<U1, U2>& other)
        noexcept(is_nothrow_assignable<T1&, const U1&>::value &&
                 is_nothrow_assignable<T2&, const U2&>::value) {
        first() = static_cast<const tuple_leaf<0, U1>&>(other).value();
        second() = static_cast<const tuple_leaf<1, U2>&>(other).value();
        return *this;
    }

    template<typename U1, typename U2,
             enable_if_t<
                 is_assignable<T1&, U1&&>::value &&
                 is_assignable<T2&, U2&&>::value,
                 int> = 0>
    tuple& operator=(tuple<U1, U2>&& other)
        noexcept(is_nothrow_assignable<T1&, U1&&>::value &&
                 is_nothrow_assignable<T2&, U2&&>::value) {
        first() = std::move(static_cast<tuple_leaf<0, U1>&>(other).value());
        second() = std::move(static_cast<tuple_leaf<1, U2>&>(other).value());
        return *this;
    }

#if __cplusplus > 202002L
    template<typename U1, typename U2>
        requires (is_assignable<const T1&, const U1&>::value &&
                  is_assignable<const T2&, const U2&>::value)
    constexpr const tuple& operator=(const tuple<U1, U2>& other) const
        noexcept(is_nothrow_assignable<const T1&, const U1&>::value &&
                 is_nothrow_assignable<const T2&, const U2&>::value) {
        first() = static_cast<const tuple_leaf<0, U1>&>(other).value();
        second() = static_cast<const tuple_leaf<1, U2>&>(other).value();
        return *this;
    }

    template<typename U1, typename U2>
        requires (is_assignable<const T1&, U1>::value &&
                  is_assignable<const T2&, U2>::value)
    constexpr const tuple& operator=(tuple<U1, U2>&& other) const
        noexcept(is_nothrow_assignable<const T1&, U1&&>::value &&
                 is_nothrow_assignable<const T2&, U2&&>::value) {
        first() = std::move(static_cast<tuple_leaf<0, U1>&>(other).value());
        second() = std::move(static_cast<tuple_leaf<1, U2>&>(other).value());
        return *this;
    }

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 const Source&, 2>::value &&
                             detail::tuple_source_assignable_if_enabled<
                                 tuple<T1, T2>, const Source&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     const Source&, 2>::value>::value,
                         int> = 0>
    constexpr tuple& operator=(const Source& source)
        noexcept(detail::tuple_source_nothrow_assignable_if_enabled<
                     tuple<T1, T2>, const Source&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         const Source&, 2>::value>::value) {
        first() = get<0>(source);
        second() = get<1>(source);
        return *this;
    }

    template<typename Source,
             enable_if_t<!is_lvalue_reference<Source>::value &&
                             detail::is_additional_tuple_like_source<
                                 Source&&, 2>::value &&
                             detail::tuple_source_assignable_if_enabled<
                                 tuple<T1, T2>, Source&&,
                                 index_sequence<0, 1>,
                                 detail::is_additional_tuple_like_source<
                                     Source&&, 2>::value>::value,
                         int> = 0>
    constexpr tuple& operator=(Source&& source)
        noexcept(detail::tuple_source_nothrow_assignable_if_enabled<
                     tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source&&, 2>::value>::value) {
        first() = get<0>(std::forward<Source>(source));
        second() = get<1>(std::forward<Source>(source));
        return *this;
    }

    template<typename Source>
        requires detail::is_additional_tuple_like_source<const Source&, 2>::value &&
                 detail::tuple_source_const_assignable_if_enabled<
                     tuple<T1, T2>, const Source&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         const Source&, 2>::value>::value
    constexpr const tuple& operator=(const Source& source) const
        noexcept(detail::tuple_source_const_nothrow_assignable_if_enabled<
                     tuple<T1, T2>, const Source&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         const Source&, 2>::value>::value) {
        first() = get<0>(source);
        second() = get<1>(source);
        return *this;
    }

    template<typename Source>
        requires (!is_lvalue_reference<Source>::value) &&
                 detail::is_additional_tuple_like_source<Source&&, 2>::value &&
                 detail::tuple_source_const_assignable_if_enabled<
                     tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source&&, 2>::value>::value
    constexpr const tuple& operator=(Source&& source) const
        noexcept(detail::tuple_source_const_nothrow_assignable_if_enabled<
                     tuple<T1, T2>, Source&&, index_sequence<0, 1>,
                     detail::is_additional_tuple_like_source<
                         Source&&, 2>::value>::value) {
        first() = get<0>(std::forward<Source>(source));
        second() = get<1>(std::forward<Source>(source));
        return *this;
    }
#endif

    /* Assignment from pair */
    template<typename U1, typename U2,
             enable_if_t<
                 is_assignable<T1&, const U1&>::value &&
                 is_assignable<T2&, const U2&>::value,
                 int> = 0>
    tuple& operator=(const pair<U1, U2>& p)
        noexcept(is_nothrow_assignable<T1&, const U1&>::value &&
                 is_nothrow_assignable<T2&, const U2&>::value) {
        first() = p.first;
        second() = p.second;
        return *this;
    }

    template<typename U1, typename U2,
             enable_if_t<
                 is_assignable<T1&, U1&&>::value &&
                 is_assignable<T2&, U2&&>::value,
                 int> = 0>
    tuple& operator=(pair<U1, U2>&& p)
        noexcept(is_nothrow_assignable<T1&, U1&&>::value &&
                 is_nothrow_assignable<T2&, U2&&>::value) {
        first() = std::move(p.first);
        second() = std::move(p.second);
        return *this;
    }

#if __cplusplus > 202002L
    template<typename U1, typename U2>
        requires (is_assignable<const T1&, const U1&>::value &&
                  is_assignable<const T2&, const U2&>::value)
    constexpr const tuple& operator=(const pair<U1, U2>& p) const
        noexcept(is_nothrow_assignable<const T1&, const U1&>::value &&
                 is_nothrow_assignable<const T2&, const U2&>::value) {
        first() = p.first;
        second() = p.second;
        return *this;
    }

    template<typename U1, typename U2>
        requires (is_assignable<const T1&, U1>::value &&
                  is_assignable<const T2&, U2>::value)
    constexpr const tuple& operator=(pair<U1, U2>&& p) const
        noexcept(is_nothrow_assignable<const T1&, U1&&>::value &&
                 is_nothrow_assignable<const T2&, U2&&>::value) {
        first() = std::move(p.first);
        second() = std::move(p.second);
        return *this;
    }
#endif

    RIN_TUPLE_CONSTEXPR14 void swap(tuple& other)
        noexcept(is_nothrow_swappable<T1>::value &&
                 is_nothrow_swappable<T2>::value) {
        base::swap(other);
    }
};

/* 空のtuple特殊化 */
template<>
class tuple<> {
public:
    constexpr tuple() = default;

    template<typename Alloc>
    constexpr tuple(allocator_arg_t, const Alloc&) {}

    template<typename Alloc>
    constexpr tuple(allocator_arg_t, const Alloc&, const tuple&) {}

    template<typename Alloc>
    constexpr tuple(allocator_arg_t, const Alloc&, tuple&&) {}

#if __cplusplus > 202002L
    template<typename Alloc, typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                              Source, 0>::value,
                          int> = 0>
    constexpr tuple(allocator_arg_t, const Alloc&, Source&&) noexcept {}

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 Source, 0>::value &&
                             detail::tuple_source_constructible<
                                 tuple<>, Source&&,
                                 index_sequence<>>::value,
                         int> = 0>
    constexpr tuple(Source&&) {}

    template<typename Source,
             enable_if_t<detail::is_additional_tuple_like_source<
                                 const Source&, 0>::value,
                         int> = 0>
    constexpr tuple& operator=(const Source&) noexcept { return *this; }

    template<typename Source,
             enable_if_t<!is_lvalue_reference<Source>::value &&
                             detail::is_additional_tuple_like_source<
                                 Source&&, 0>::value,
                         int> = 0>
    constexpr tuple& operator=(Source&&) noexcept { return *this; }

    constexpr tuple& operator=(const tuple&) noexcept { return *this; }
    constexpr tuple& operator=(tuple&&) noexcept { return *this; }

    template<typename Source>
        requires detail::is_additional_tuple_like_source<const Source&, 0>::value
    constexpr const tuple& operator=(const Source&) const noexcept {
        return *this;
    }

    template<typename Source>
        requires (!is_lvalue_reference<Source>::value) &&
                 detail::is_additional_tuple_like_source<Source&&, 0>::value
    constexpr const tuple& operator=(Source&&) const noexcept {
        return *this;
    }

    constexpr const tuple& operator=(const tuple&) const {
        return *this;
    }

    constexpr const tuple& operator=(tuple&&) const {
        return *this;
    }
#endif

    RIN_TUPLE_CONSTEXPR14 void swap(tuple&) noexcept {}
};

#if __cplusplus > 202002L
namespace detail {

/* C++23 common_type is available only when the tuple arities agree and every
 * element pair has a common type.  Keeping the probe in void_t preserves
 * SFINAE for unsupported element pairs instead of exposing a malformed
 * tuple<common_type_t<...>>. */
template<typename Left, typename Right, bool SameArity, typename = void>
struct tuple_common_type {};

template<typename... Left, typename... Right>
struct tuple_common_type<
    tuple<Left...>, tuple<Right...>, true,
    void_t<common_type_t<Left, Right>...>> {
    using type = tuple<common_type_t<Left, Right>...>;
};

} /* namespace detail */

template<typename... Left, typename... Right>
struct common_type<tuple<Left...>, tuple<Right...>>
    : detail::tuple_common_type<
          tuple<Left...>, tuple<Right...>,
          sizeof...(Left) == sizeof...(Right)> {};

namespace detail {

/* C++23 common_reference carries each operand's cv/ref qualifier template
 * through to its elements, then requires every element pair to provide a
 * common reference.  As with common_type, arity mismatch and unsupported
 * elements intentionally leave type absent. */
template<typename Left, typename Right, bool SameArity,
         template<typename> class LeftQual,
         template<typename> class RightQual, typename = void>
struct tuple_basic_common_reference {};

template<typename... Left, typename... Right,
         template<typename> class LeftQual,
         template<typename> class RightQual>
struct tuple_basic_common_reference<
    tuple<Left...>, tuple<Right...>, true, LeftQual, RightQual,
    void_t<common_reference_t<LeftQual<Left>, RightQual<Right>>...>> {
    using type = tuple<common_reference_t<LeftQual<Left>, RightQual<Right>>...>;
};

} /* namespace detail */

template<typename... Left, typename... Right,
         template<typename> class LeftQual,
         template<typename> class RightQual>
struct basic_common_reference<tuple<Left...>, tuple<Right...>,
                              LeftQual, RightQual>
    : detail::tuple_basic_common_reference<
          tuple<Left...>, tuple<Right...>,
          sizeof...(Left) == sizeof...(Right), LeftQual, RightQual> {};
#endif

template<typename... Types,
         enable_if_t<conjunction<is_swappable<Types>...>::value, int> = 0>
RIN_TUPLE_CONSTEXPR14 void swap(tuple<Types...>& left, tuple<Types...>& right)
    noexcept(conjunction<is_nothrow_swappable<Types>...>::value) {
    left.swap(right);
}

/* ═══════════════════════════════════════════════════════════════
 * Deduction guides (C++17 CTAD)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename... Types>
tuple(Types...) -> tuple<Types...>;
#endif

/* ═══════════════════════════════════════════════════════════════
 * tuple_size
 * ═══════════════════════════════════════════════════════════════*/

template<typename... Types>
struct tuple_size<tuple<Types...>>
    : integral_constant<size_t, sizeof...(Types)> {};

/* cv-qualified forwarding for tuple */
template<typename... Types>
struct tuple_size<const tuple<Types...>>
    : integral_constant<size_t, sizeof...(Types)> {};

template<typename... Types>
struct tuple_size<volatile tuple<Types...>>
    : integral_constant<size_t, sizeof...(Types)> {};

template<typename... Types>
struct tuple_size<const volatile tuple<Types...>>
    : integral_constant<size_t, sizeof...(Types)> {};

/* ═══════════════════════════════════════════════════════════════
 * tuple_element
 * ═══════════════════════════════════════════════════════════════*/

template<size_t I, typename Head, typename... Tail>
struct tuple_element<I, tuple<Head, Tail...>>
    : tuple_element<I - 1, tuple<Tail...>> {};

template<typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
    using type = Head;
};

/* cv-qualified forwarding for tuple_element */
template<size_t I, typename... Types>
struct tuple_element<I, const tuple<Types...>> {
    using type = const typename tuple_element<I, tuple<Types...>>::type;
};

template<size_t I, typename... Types>
struct tuple_element<I, volatile tuple<Types...>> {
    using type = volatile typename tuple_element<I, tuple<Types...>>::type;
};

template<size_t I, typename... Types>
struct tuple_element<I, const volatile tuple<Types...>> {
    using type = const volatile typename tuple_element<I, tuple<Types...>>::type;
};

/* tuple_size_v (C++17) - general definition */
#if __cplusplus >= 201703L
template<class T>
inline constexpr size_t tuple_size_v = tuple_size<T>::value;
#endif

/* ═══════════════════════════════════════════════════════════════
 * get<I> - tuple_leafから直接取得
 * ═══════════════════════════════════════════════════════════════*/

/* tuple_leafを使って直接アクセス */
template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&
get(tuple<Types...>& t) noexcept {
    return static_cast<
        tuple_leaf<I, tuple_element_t<I, tuple<Types...>>>&>(t).value();
}

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&
get(const tuple<Types...>& t) noexcept {
    return static_cast<const
        tuple_leaf<I, tuple_element_t<I, tuple<Types...>>>&>(t).value();
}

template<size_t I, typename... Types>
constexpr tuple_element_t<I, tuple<Types...>>&&
get(tuple<Types...>&& t) noexcept {
    return static_cast<tuple_element_t<I, tuple<Types...>>&&>(
        static_cast<
            tuple_leaf<I, tuple_element_t<I, tuple<Types...>>>&>(t).value());
}

template<size_t I, typename... Types>
constexpr const tuple_element_t<I, tuple<Types...>>&&
get(const tuple<Types...>&& t) noexcept {
    return static_cast<const tuple_element_t<I, tuple<Types...>>&&>(
        static_cast<const
            tuple_leaf<I, tuple_element_t<I, tuple<Types...>>>&>(t).value());
}

/* ═══════════════════════════════════════════════════════════════
 * get<T> - 型によるアクセス (C++14)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201402L
namespace detail {

/* A type-based get participates only for an exact, unique element type. */
template<typename T, typename... Types>
struct tuple_type_count;

template<typename T>
struct tuple_type_count<T> : integral_constant<size_t, 0> {};

template<typename T, typename First, typename... Rest>
struct tuple_type_count<T, First, Rest...>
    : integral_constant<size_t,
        (is_same<T, First>::value ? 1 : 0) +
        tuple_type_count<T, Rest...>::value> {};

/* Find the first index after uniqueness has been established. */
template<typename T, typename... Types>
struct tuple_index;

template<typename T, typename First, typename... Rest>
struct tuple_index<T, First, Rest...>
    : integral_constant<size_t, is_same<T, First>::value ? 0 : 1 + tuple_index<T, Rest...>::value> {};

template<typename T>
struct tuple_index<T> : integral_constant<size_t, 0> {};

} /* namespace detail */

template<typename T, typename... Types,
         enable_if_t<detail::tuple_type_count<T, Types...>::value == 1,
                     int> = 0>
constexpr T& get(tuple<Types...>& t) noexcept {
    return get<detail::tuple_index<T, Types...>::value>(t);
}

template<typename T, typename... Types,
         enable_if_t<detail::tuple_type_count<T, Types...>::value == 1,
                     int> = 0>
constexpr const T& get(const tuple<Types...>& t) noexcept {
    return get<detail::tuple_index<T, Types...>::value>(t);
}

template<typename T, typename... Types,
         enable_if_t<detail::tuple_type_count<T, Types...>::value == 1,
                     int> = 0>
constexpr T&& get(tuple<Types...>&& t) noexcept {
    return get<detail::tuple_index<T, Types...>::value>(std::move(t));
}

template<typename T, typename... Types,
         enable_if_t<detail::tuple_type_count<T, Types...>::value == 1,
                     int> = 0>
constexpr const T&& get(const tuple<Types...>&& t) noexcept {
    return get<detail::tuple_index<T, Types...>::value>(std::move(t));
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * make_tuple
 * ═══════════════════════════════════════════════════════════════*/

template<typename... Types>
constexpr tuple<unwrap_ref_decay_t<Types>...> make_tuple(Types&&... args) {
    return tuple<unwrap_ref_decay_t<Types>...>(std::forward<Types>(args)...);
}

/* ═══════════════════════════════════════════════════════════════
 * tie
 * ═══════════════════════════════════════════════════════════════*/

template<typename... Types>
tuple<Types&...> tie(Types&... args) noexcept {
    return tuple<Types&...>(args...);
}

/* ignore placeholder */
struct ignore_t {
    template<typename T>
    const ignore_t& operator=(const T&) const noexcept { return *this; }
};

#if __cplusplus >= 201703L
inline constexpr ignore_t ignore{};
#else
static const ignore_t ignore = ignore_t();
#endif

/* ═══════════════════════════════════════════════════════════════
 * forward_as_tuple
 * ═══════════════════════════════════════════════════════════════*/

template<typename... Types>
tuple<Types&&...> forward_as_tuple(Types&&... args) noexcept {
    return tuple<Types&&...>(std::forward<Types>(args)...);
}

/* ═══════════════════════════════════════════════════════════════
 * tuple比較 - ヘルパー使用
 * ═══════════════════════════════════════════════════════════════*/

template<typename T1, typename T2, size_t I, size_t N>
struct tuple_compare_impl {
    static constexpr bool equal(const T1& lhs, const T2& rhs) {
        return get<I>(lhs) == get<I>(rhs) && tuple_compare_impl<T1, T2, I + 1, N>::equal(lhs, rhs);
    }
    static RIN_TUPLE_CONSTEXPR14 bool less(const T1& lhs, const T2& rhs) {
        if (get<I>(lhs) < get<I>(rhs)) return true;
        if (get<I>(rhs) < get<I>(lhs)) return false;
        return tuple_compare_impl<T1, T2, I + 1, N>::less(lhs, rhs);
    }
};

template<typename T1, typename T2, size_t N>
struct tuple_compare_impl<T1, T2, N, N> {
    static constexpr bool equal(const T1&, const T2&) { return true; }
    static constexpr bool less(const T1&, const T2&) { return false; }
};

#if __cplusplus >= 202002L
namespace detail {

/* C++20 tuple ordering uses synth-three-way rather than requiring every
 * element to spell <=>.  This preserves standard lexicographic comparison
 * for legacy types that expose only a bidirectional < relation. */
template<typename Left, typename Right, typename = void>
struct tuple_three_way_category {};

template<typename... Left, typename... Right>
struct tuple_three_way_category<
    tuple<Left...>, tuple<Right...>,
    enable_if_t<sizeof...(Left) == sizeof...(Right),
                void_t<common_comparison_category_t<
                    synth_three_way_result_t<Left, Right>...>>>> {
    using type = common_comparison_category_t<
        synth_three_way_result_t<Left, Right>...>;
};

template<typename Left, typename Right, size_t I, size_t N>
struct tuple_three_way_impl {
    template<typename Category>
    static constexpr Category compare(const Left& left, const Right& right)
        noexcept(noexcept(synth_three_way(get<I>(left), get<I>(right))) &&
                 noexcept(tuple_three_way_impl<Left, Right, I + 1, N>::
                              template compare<Category>(left, right))) {
        const auto result = synth_three_way(get<I>(left), get<I>(right));
        if (result != 0) return static_cast<Category>(result);
        return tuple_three_way_impl<Left, Right, I + 1, N>::
            template compare<Category>(left, right);
    }
};

template<typename Left, typename Right, size_t N>
struct tuple_three_way_impl<Left, Right, N, N> {
    template<typename Category>
    static constexpr Category compare(const Left&, const Right&) noexcept {
        return Category::equivalent;
    }
};

} /* namespace detail */
#endif

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
RIN_TUPLE_CONSTEXPR14 bool
operator==(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return tuple_compare_impl<tuple<T1...>, tuple<T2...>, 0, sizeof...(T1)>::equal(lhs, rhs);
}

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
constexpr bool operator!=(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return !(lhs == rhs);
}

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
RIN_TUPLE_CONSTEXPR14 bool
operator<(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return tuple_compare_impl<tuple<T1...>, tuple<T2...>, 0, sizeof...(T1)>::less(lhs, rhs);
}

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
constexpr bool operator>(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return rhs < lhs;
}

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
constexpr bool operator<=(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return !(rhs < lhs);
}

template<typename... T1, typename... T2,
         enable_if_t<sizeof...(T1) == sizeof...(T2), int> = 0>
constexpr bool operator>=(const tuple<T1...>& lhs, const tuple<T2...>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
template<typename... Left, typename... Right>
    requires (sizeof...(Left) == sizeof...(Right)) &&
             requires {
                 typename detail::tuple_three_way_category<
                     tuple<Left...>, tuple<Right...>>::type;
             }
constexpr auto operator<=>(const tuple<Left...>& left,
                           const tuple<Right...>& right)
    noexcept(noexcept(detail::tuple_three_way_impl<
                 tuple<Left...>, tuple<Right...>, 0, sizeof...(Left)>::
                     template compare<typename detail::tuple_three_way_category<
                         tuple<Left...>, tuple<Right...>>::type>(left, right)))
    -> typename detail::tuple_three_way_category<
        tuple<Left...>, tuple<Right...>>::type {
    using category = typename detail::tuple_three_way_category<
        tuple<Left...>, tuple<Right...>>::type;
    return detail::tuple_three_way_impl<tuple<Left...>, tuple<Right...>,
                                        0, sizeof...(Left)>::
        template compare<category>(left, right);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * tuple_cat - concatenate tuples
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {
// tuple_cat helpers

/* tuple_cat only participates for tuple-like sources.  The previous
 * implementation formed tuple_size/get expressions directly in the return
 * type, so a non tuple-like argument could leak a hard error from a helper
 * instead of being rejected by overload resolution.  Probe the complete
 * source shape first, including every get<I> expression; this keeps ADL
 * tuple-like customizations usable while making malformed sources SFINAE
 * friendly. */
template<typename Tuple, typename Indices, typename = void>
struct tuple_cat_source_impl : false_type {};

template<typename Tuple, size_t... Is>
struct tuple_cat_source_impl<
    Tuple, index_sequence<Is...>,
    void_t<decltype(get<Is>(declval<Tuple>()))...,
           tuple_element_t<Is, remove_cvref_t<Tuple>>...>> : true_type {};

template<typename Tuple, typename = void>
struct tuple_cat_source : false_type {};

template<typename Tuple>
struct tuple_cat_source<
    Tuple, void_t<decltype(tuple_size<remove_cvref_t<Tuple>>::value)>>
    : tuple_cat_source_impl<
          Tuple,
          make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>> {};

template<typename... Tuples>
struct tuple_cat_all_sources : conjunction<tuple_cat_source<Tuples>...> {};

template<typename Tuple, size_t... Is>
constexpr auto tuple_cat_one_impl(Tuple&& t, index_sequence<Is...>)
    -> tuple<tuple_element_t<Is, remove_cvref_t<Tuple>>...> {
    return tuple<tuple_element_t<Is, remove_cvref_t<Tuple>>...>(
        get<Is>(std::forward<Tuple>(t))...);
}

template<typename Tuple,
         typename enable_if<tuple_cat_source<Tuple&&>::value, int>::type = 0>
constexpr auto tuple_cat_one(Tuple&& t)
    -> decltype(tuple_cat_one_impl(
        std::forward<Tuple>(t),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>{})) {
    return tuple_cat_one_impl(
        std::forward<Tuple>(t),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>{});
}

template<typename Tuple1, typename Tuple2, size_t... I1, size_t... I2>
constexpr auto tuple_cat_two_impl(Tuple1&& t1, Tuple2&& t2,
                                  index_sequence<I1...>, index_sequence<I2...>)
    -> tuple<tuple_element_t<I1, remove_cvref_t<Tuple1>>...,
             tuple_element_t<I2, remove_cvref_t<Tuple2>>...> {
    return tuple<tuple_element_t<I1, remove_cvref_t<Tuple1>>...,
                 tuple_element_t<I2, remove_cvref_t<Tuple2>>...>(
        get<I1>(std::forward<Tuple1>(t1))...,
        get<I2>(std::forward<Tuple2>(t2))...);
}

template<typename Tuple1, typename Tuple2,
         typename enable_if<
             tuple_cat_source<Tuple1&&>::value &&
                 tuple_cat_source<Tuple2&&>::value,
             int>::type = 0>
constexpr auto tuple_cat_two(Tuple1&& t1, Tuple2&& t2)
    -> decltype(tuple_cat_two_impl(
        std::forward<Tuple1>(t1), std::forward<Tuple2>(t2),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple1>>::value>{},
        make_index_sequence<tuple_size<remove_cvref_t<Tuple2>>::value>{})) {
    return tuple_cat_two_impl(
        std::forward<Tuple1>(t1), std::forward<Tuple2>(t2),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple1>>::value>{},
        make_index_sequence<tuple_size<remove_cvref_t<Tuple2>>::value>{});
}

} // namespace detail

// Empty tuple
inline constexpr tuple<> tuple_cat() { return tuple<>(); }

// Single tuple
template<typename Tuple,
         typename enable_if<detail::tuple_cat_source<Tuple&&>::value,
                            int>::type = 0>
constexpr auto tuple_cat(Tuple&& t)
    -> decltype(detail::tuple_cat_one(std::forward<Tuple>(t))) {
    return detail::tuple_cat_one(std::forward<Tuple>(t));
}

// Two tuples
template<typename Tuple1, typename Tuple2,
         typename enable_if<
             detail::tuple_cat_source<Tuple1&&>::value &&
                 detail::tuple_cat_source<Tuple2&&>::value,
             int>::type = 0>
constexpr auto tuple_cat(Tuple1&& t1, Tuple2&& t2)
    -> decltype(detail::tuple_cat_two(
        std::forward<Tuple1>(t1), std::forward<Tuple2>(t2))) {
    return detail::tuple_cat_two(std::forward<Tuple1>(t1),
                                 std::forward<Tuple2>(t2));
}

// Three or more tuples
template<typename Tuple1, typename Tuple2, typename Tuple3, typename... Rest,
         typename enable_if<detail::tuple_cat_all_sources<
                                Tuple1&&, Tuple2&&, Tuple3&&,
                                Rest&&...>::value,
                            int>::type = 0>
constexpr auto tuple_cat(Tuple1&& t1, Tuple2&& t2, Tuple3&& t3,
                         Rest&&... rest)
    -> decltype(tuple_cat(
        detail::tuple_cat_two(std::forward<Tuple1>(t1),
                              std::forward<Tuple2>(t2)),
        std::forward<Tuple3>(t3), std::forward<Rest>(rest)...)) {
    return tuple_cat(detail::tuple_cat_two(std::forward<Tuple1>(t1),
                                           std::forward<Tuple2>(t2)),
                     std::forward<Tuple3>(t3), std::forward<Rest>(rest)...);
}

/* ═══════════════════════════════════════════════════════════════
 * apply (C++17) - 関数にtupleの要素を展開して適用
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
namespace detail {

template<typename F, typename Tuple, size_t... Is>
constexpr decltype(auto) apply_impl(F&& f, Tuple&& t, index_sequence<Is...>) {
    return std::invoke(std::forward<F>(f),
                       get<Is>(std::forward<Tuple>(t))...);
}

} // namespace detail

template<typename F, typename Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t) {
    return detail::apply_impl(
        std::forward<F>(f),
        std::forward<Tuple>(t),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>{}
    );
}

/* ═══════════════════════════════════════════════════════════════
 * make_from_tuple (C++17) - tupleの要素でオブジェクトを構築
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T, typename Tuple, size_t... Is>
constexpr T make_from_tuple_impl(Tuple&& t, index_sequence<Is...>) {
    return T(get<Is>(std::forward<Tuple>(t))...);
}

} // namespace detail

template<typename T, typename Tuple>
constexpr T make_from_tuple(Tuple&& t) {
    return detail::make_from_tuple_impl<T>(
        std::forward<Tuple>(t),
        make_index_sequence<tuple_size<remove_cvref_t<Tuple>>::value>{}
    );
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * pair piecewise_construct コンストラクタ実装
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename T1, typename T2, typename Tuple1, typename Tuple2, size_t... I1, size_t... I2>
constexpr pair<T1, T2> make_pair_piecewise_impl(
    Tuple1&& t1, Tuple2&& t2,
    index_sequence<I1...>, index_sequence<I2...>) {
    return pair<T1, T2>(
        get<I1>(std::forward<Tuple1>(t1))...,
        get<I2>(std::forward<Tuple2>(t2))...
    );
}

} /* namespace detail */

} /* namespace std */

#endif /* __cplusplus */

#undef RIN_TUPLE_CONSTEXPR14

#endif /* RINCXX_TUPLE_H */
