/*
 * RinOS C++ <numeric> ✿
 * 数値アルゴリズム
 */

#ifndef RINCXX_NUMERIC_H
#define RINCXX_NUMERIC_H

#include "rincxx.h"
#include "version.h"
#include "iterator.h"
#include "functional.h"
#include "limits.h"
#include "utility.h"

#if __cplusplus >= 201703L
#include "execution.h"
#endif

#if __cplusplus > 202002L
#include "ranges.h"
#include "optional.h"
#endif

/* C++26 checked integer arithmetic is also a <numeric> facility.  Keep the
 * C compatibility owner in one header so <numeric> and direct
 * <stdckdint.h> users observe the same pointer/result contract. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_NUMERIC)
#include "stdckdint.h"
#endif

#ifdef __cplusplus

namespace std {

#if __cplusplus >= 202002L
#define RIN_NUMERIC_CONSTEXPR20 constexpr
#else
#define RIN_NUMERIC_CONSTEXPR20 inline
#endif

/* ═══════════════════════════════════════════════════════════════
 * accumulate
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename T>
RIN_NUMERIC_CONSTEXPR20 T accumulate(InputIt first, InputIt last, T init) {
    for (; first != last; ++first) {
        init = std::move(init) + *first;
    }
    return init;
}

template<typename InputIt, typename T, typename BinaryOp>
RIN_NUMERIC_CONSTEXPR20 T accumulate(InputIt first, InputIt last, T init, BinaryOp op) {
    for (; first != last; ++first) {
        init = op(std::move(init), *first);
    }
    return init;
}

/* ═══════════════════════════════════════════════════════════════
 * reduce (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename InputIt, typename T, typename BinaryOp>
constexpr T reduce(InputIt first, InputIt last, T init, BinaryOp op) {
    for (; first != last; ++first) {
        init = op(std::move(init), *first);
    }
    return init;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * inner_product
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt1, typename InputIt2, typename T>
RIN_NUMERIC_CONSTEXPR20 T inner_product(InputIt1 first1, InputIt1 last1,
                                        InputIt2 first2, T init) {
    for (; first1 != last1; ++first1, ++first2) {
        init = std::move(init) + (*first1) * (*first2);
    }
    return init;
}

template<typename InputIt1, typename InputIt2, typename T, typename BinaryOp1, typename BinaryOp2>
RIN_NUMERIC_CONSTEXPR20 T inner_product(InputIt1 first1, InputIt1 last1,
                                        InputIt2 first2, T init,
                                        BinaryOp1 op1, BinaryOp2 op2) {
    for (; first1 != last1; ++first1, ++first2) {
        init = op1(std::move(init), op2(*first1, *first2));
    }
    return init;
}

/* ═══════════════════════════════════════════════════════════════
 * adjacent_difference
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename OutputIt>
RIN_NUMERIC_CONSTEXPR20 OutputIt adjacent_difference(InputIt first,
                                                      InputIt last,
                                                      OutputIt d_first) {
    if (first == last) return d_first;
    
    using value_type = typename iterator_traits<InputIt>::value_type;
    value_type acc = *first;
    *d_first = acc;
    ++d_first;
    
    while (++first != last) {
        value_type val = *first;
        *d_first++ = val - std::move(acc);
        acc = std::move(val);
    }
    return d_first;
}

template<typename InputIt, typename OutputIt, typename BinaryOp>
RIN_NUMERIC_CONSTEXPR20 OutputIt adjacent_difference(InputIt first,
                                                      InputIt last,
                                                      OutputIt d_first,
                                                      BinaryOp op) {
    if (first == last) return d_first;
    
    using value_type = typename iterator_traits<InputIt>::value_type;
    value_type acc = *first;
    *d_first = acc;
    ++d_first;
    
    while (++first != last) {
        value_type val = *first;
        *d_first++ = op(val, std::move(acc));
        acc = std::move(val);
    }
    return d_first;
}

/* ═══════════════════════════════════════════════════════════════
 * partial_sum
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename OutputIt>
RIN_NUMERIC_CONSTEXPR20 OutputIt partial_sum(InputIt first, InputIt last,
                                              OutputIt d_first) {
    if (first == last) return d_first;
    
    typename iterator_traits<InputIt>::value_type sum = *first;
    *d_first = sum;
    
    while (++first != last) {
        sum = std::move(sum) + *first;
        *++d_first = sum;
    }
    return ++d_first;
}

template<typename InputIt, typename OutputIt, typename BinaryOp>
RIN_NUMERIC_CONSTEXPR20 OutputIt partial_sum(InputIt first, InputIt last,
                                              OutputIt d_first, BinaryOp op) {
    if (first == last) return d_first;
    
    typename iterator_traits<InputIt>::value_type sum = *first;
    *d_first = sum;
    
    while (++first != last) {
        sum = op(std::move(sum), *first);
        *++d_first = sum;
    }
    return ++d_first;
}

/* ═══════════════════════════════════════════════════════════════
 * inclusive_scan (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
template<typename InputIt, typename OutputIt>
constexpr OutputIt inclusive_scan(InputIt first, InputIt last,
                                  OutputIt d_first) {
    return partial_sum(first, last, d_first);
}

template<typename InputIt, typename OutputIt, typename BinaryOp>
constexpr OutputIt inclusive_scan(InputIt first, InputIt last,
                                  OutputIt d_first, BinaryOp op) {
    return partial_sum(first, last, d_first, op);
}

template<typename InputIt, typename OutputIt, typename BinaryOp, typename T>
constexpr OutputIt inclusive_scan(InputIt first, InputIt last,
                                  OutputIt d_first, BinaryOp op, T init) {
    for (; first != last; ++first) {
        init = op(std::move(init), *first);
        *d_first++ = init;
    }
    return d_first;
}

/* ═══════════════════════════════════════════════════════════════
 * exclusive_scan (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename OutputIt, typename T>
constexpr OutputIt exclusive_scan(InputIt first, InputIt last,
                                  OutputIt d_first, T init) {
    return exclusive_scan(first, last, d_first, init, std::plus<>());
}

template<typename InputIt, typename OutputIt, typename T, typename BinaryOp>
constexpr OutputIt exclusive_scan(InputIt first, InputIt last,
                                  OutputIt d_first, T init, BinaryOp op) {
    for (; first != last; ++first) {
        auto value = *first;
        *d_first++ = init;
        init = op(std::move(init), std::move(value));
    }
    return d_first;
}

/* ═══════════════════════════════════════════════════════════════
 * transform_reduce (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt1, typename InputIt2, typename T>
constexpr T transform_reduce(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                             T init) {
    return inner_product(first1, last1, first2, init);
}

template<typename InputIt1, typename InputIt2, typename T, typename BinaryReduce, typename BinaryTransform>
constexpr T transform_reduce(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                             T init, BinaryReduce reduce,
                             BinaryTransform transform) {
    return inner_product(first1, last1, first2, init, reduce, transform);
}

template<typename InputIt, typename T, typename BinaryReduce, typename UnaryTransform>
constexpr T transform_reduce(InputIt first, InputIt last, T init,
                             BinaryReduce reduce, UnaryTransform transform) {
    for (; first != last; ++first) {
        init = reduce(std::move(init), transform(*first));
    }
    return init;
}

template<typename InputIt, typename T>
constexpr T reduce(InputIt first, InputIt last, T init) {
    return reduce(first, last, init, std::plus<>());
}

template<typename InputIt>
constexpr typename iterator_traits<InputIt>::value_type
reduce(InputIt first, InputIt last) {
    return reduce(first, last, typename iterator_traits<InputIt>::value_type{});
}

/* ═══════════════════════════════════════════════════════════════
 * transform_inclusive_scan / transform_exclusive_scan (C++17)
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename OutputIt, typename BinaryOp,
         typename UnaryOp>
constexpr OutputIt transform_inclusive_scan(InputIt first, InputIt last,
                                            OutputIt d_first,
                                            BinaryOp binary_op,
                                            UnaryOp unary_op) {
    if (first == last) return d_first;
    auto init = unary_op(*first);
    *d_first++ = init;
    while (++first != last) {
        init = binary_op(std::move(init), unary_op(*first));
        *d_first++ = init;
    }
    return d_first;
}

template<typename InputIt, typename OutputIt, typename BinaryOp,
         typename UnaryOp, typename T>
constexpr OutputIt transform_inclusive_scan(InputIt first, InputIt last,
                                            OutputIt d_first,
                                            BinaryOp binary_op,
                                            UnaryOp unary_op, T init) {
    for (; first != last; ++first) {
        init = binary_op(std::move(init), unary_op(*first));
        *d_first++ = init;
    }
    return d_first;
}

template<typename InputIt, typename OutputIt, typename T,
         typename BinaryOp, typename UnaryOp>
constexpr OutputIt transform_exclusive_scan(InputIt first, InputIt last,
                                            OutputIt d_first, T init,
                                            BinaryOp binary_op,
                                            UnaryOp unary_op) {
    for (; first != last; ++first) {
        auto value = unary_op(*first);
        *d_first++ = init;
        init = binary_op(std::move(init), std::move(value));
    }
    return d_first;
}
#endif

#if __cplusplus >= 201703L
/*
 * Execution-policy exception boundary.  RinOS currently has no scheduler-
 * backed parallel algorithm owner, so all policies still use the checked
 * sequential core.  The policy exception contract is nevertheless kept
 * distinct: sequenced execution propagates a callable exception, while the
 * parallel and unsequenced policies terminate as required by the standard.
 */
namespace __numeric_detail {

template<class Policy>
struct policy_terminates_on_exception : false_type {};

template<>
struct policy_terminates_on_exception<execution::parallel_policy> : true_type {};

template<>
struct policy_terminates_on_exception<execution::parallel_unsequenced_policy>
    : true_type {};

#if __cplusplus >= 202002L
template<>
struct policy_terminates_on_exception<execution::unsequenced_policy>
    : true_type {};
#endif

template<class Policy, class Function>
inline auto invoke_policy(Policy&& policy, Function&& function)
    -> decltype(std::forward<Function>(function)()) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return std::forward<Function>(function)();
    } catch (...) {
        using policy_type = remove_cv_t<remove_reference_t<Policy>>;
        if (policy_terminates_on_exception<policy_type>::value)
            std::terminate();
        throw;
    }
#else
    (void)policy;
    return std::forward<Function>(function)();
#endif
}

} /* namespace __numeric_detail */

/*
 * Execution-policy overloads.  The current freestanding runtime has no
 * scheduler-backed parallel algorithm owner, so every policy is deliberately
 * evaluated through the already-checked sequential implementation.  Policy
 * exception handling remains standard-conforming even while scheduling is
 * unavailable: seq propagates, and par/unsequenced terminate.
 */
template<typename ExecutionPolicy, typename InputIt,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline typename iterator_traits<InputIt>::value_type
reduce(ExecutionPolicy&& policy, InputIt first, InputIt last) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::reduce(first, last); });
}

template<typename ExecutionPolicy, typename InputIt, typename T,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline T reduce(ExecutionPolicy&& policy, InputIt first, InputIt last, T init) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::reduce(first, last, std::move(init)); });
}

template<typename ExecutionPolicy, typename InputIt, typename T,
         typename BinaryOp,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline T reduce(ExecutionPolicy&& policy, InputIt first, InputIt last, T init,
                BinaryOp op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::reduce(first, last, std::move(init), op); });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename T,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline T transform_reduce(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
                          InputIt2 first2, T init) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_reduce(first1, last1, first2,
                                              std::move(init)); });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename T, typename BinaryReduce, typename BinaryTransform,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline T transform_reduce(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
                          InputIt2 first2, T init, BinaryReduce reduce_op,
                          BinaryTransform transform_op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_reduce(first1, last1, first2,
                                              std::move(init), reduce_op,
                                              transform_op); });
}

template<typename ExecutionPolicy, typename InputIt, typename T,
         typename BinaryReduce, typename UnaryTransform,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline T transform_reduce(ExecutionPolicy&& policy, InputIt first, InputIt last,
                          T init, BinaryReduce reduce_op,
                          UnaryTransform transform_op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_reduce(first, last, std::move(init),
                                              reduce_op, transform_op); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt inclusive_scan(ExecutionPolicy&& policy, InputIt first, InputIt last,
                               OutputIt output) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::inclusive_scan(first, last, output); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename BinaryOp,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt inclusive_scan(ExecutionPolicy&& policy, InputIt first, InputIt last,
                               OutputIt output, BinaryOp op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::inclusive_scan(first, last, output, op); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename BinaryOp, typename T,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt inclusive_scan(ExecutionPolicy&& policy, InputIt first, InputIt last,
                               OutputIt output, BinaryOp op, T init) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::inclusive_scan(first, last, output, op,
                                           std::move(init)); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename T,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt exclusive_scan(ExecutionPolicy&& policy, InputIt first, InputIt last,
                               OutputIt output, T init) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::exclusive_scan(first, last, output,
                                           std::move(init)); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename T, typename BinaryOp,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt exclusive_scan(ExecutionPolicy&& policy, InputIt first, InputIt last,
                               OutputIt output, T init, BinaryOp op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::exclusive_scan(first, last, output,
                                           std::move(init), op); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename BinaryOp, typename UnaryOp,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt transform_inclusive_scan(ExecutionPolicy&& policy,
                                         InputIt first, InputIt last,
                                         OutputIt output, BinaryOp binary_op,
                                         UnaryOp unary_op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_inclusive_scan(first, last, output,
                                                     binary_op, unary_op); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename BinaryOp, typename UnaryOp, typename T,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt transform_inclusive_scan(ExecutionPolicy&& policy,
                                         InputIt first, InputIt last,
                                         OutputIt output, BinaryOp binary_op,
                                         UnaryOp unary_op, T init) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_inclusive_scan(first, last, output,
                                                     binary_op, unary_op,
                                                     std::move(init)); });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename T, typename BinaryOp, typename UnaryOp,
         enable_if_t<is_execution_policy<remove_cv_t<
             remove_reference_t<ExecutionPolicy>>>::value, int> = 0>
inline OutputIt transform_exclusive_scan(ExecutionPolicy&& policy,
                                         InputIt first, InputIt last,
                                         OutputIt output, T init,
                                         BinaryOp binary_op,
                                         UnaryOp unary_op) {
    return __numeric_detail::invoke_policy(
        std::forward<ExecutionPolicy>(policy),
        [&]() { return std::transform_exclusive_scan(first, last, output,
                                                     std::move(init),
                                                     binary_op, unary_op); });
}
#endif /* C++17 */

/* ═══════════════════════════════════════════════════════════════
 * iota
 * ═══════════════════════════════════════════════════════════════*/

template<typename ForwardIt, typename T>
RIN_NUMERIC_CONSTEXPR20 void iota(ForwardIt first, ForwardIt last, T value) {
    for (; first != last; ++first, ++value) {
        *first = value;
    }
}

#if __cplusplus > 202002L
namespace ranges {

/* C++23 range algorithm result used by ranges::iota.  The standard names
 * this result `in_value_result`, so callers observe the `.in` member just as
 * they do for the other input/range algorithms.  Keep the converting
 * operators here because the result is an aggregate that callers may widen
 * to a different iterator or value type. */
template<typename I, typename T>
struct in_value_result {
    I in;
    T value;

    template<typename I2, typename T2>
    requires convertible_to<const I&, I2> && convertible_to<const T&, T2>
    constexpr operator in_value_result<I2, T2>() const & {
        return {in, value};
    }

    template<typename I2, typename T2>
    requires convertible_to<I, I2> && convertible_to<T, T2>
    constexpr operator in_value_result<I2, T2>() && {
        return {std::move(in), std::move(value)};
    }
};

template<typename O, typename T>
using iota_result = in_value_result<O, T>;

namespace iota_detail {

/* The value parameter follows the standard `weakly_incrementable` contract.
 * In particular, an incrementable value must expose the associated signed
 * `iter_difference_t`; accepting an otherwise incrementable type without
 * that associated type makes the CPO appear callable while violating the
 * ranges algorithm's constraint surface.  <iterator> supplies the standard
 * ptrdiff_t mapping for integral values, which are not iterators themselves. */
template<typename T>
concept value_incrementable = weakly_incrementable<T>;

} /* namespace iota_detail */

struct iota_fn {
    template<input_or_output_iterator O, typename S, typename T>
    requires sentinel_for<S, O> && iota_detail::value_incrementable<T> &&
        indirectly_writable<O, const T&>
    constexpr iota_result<O, T> operator()(O first, S last, T value) const {
        while (first != last) {
            *first = value;
            ++first;
            ++value;
        }
        return {std::move(first), std::move(value)};
    }

    template<typename R, typename T>
    requires range<R> && sentinel_for<sentinel_t<R>, iterator_t<R>> &&
        iota_detail::value_incrementable<T> &&
        indirectly_writable<iterator_t<R>, const T&>
    constexpr iota_result<borrowed_iterator_t<R>, T>
    operator()(R&& range, T value) const {
        auto result = (*this)(ranges::begin(range), ranges::end(range),
                              std::move(value));
        if constexpr (borrowed_range<R>) {
            return {std::move(result.in), std::move(result.value)};
        } else {
            return {dangling{}, std::move(result.value)};
        }
    }
};

inline constexpr iota_fn iota{};

/* C++23 fold algorithms.  Keep the operation allocation-free and preserve
 * the standard's left/right association while constraining the callable
 * before any iterator expression is instantiated. */
template<typename I, typename T>
using fold_left_with_iter_result = in_value_result<I, T>;

template<typename I, typename T>
using fold_left_first_with_iter_result =
    in_value_result<I, optional<T>>;

namespace numeric_fold_detail {

template<typename T, typename I, typename F>
using left_value_t = decay_t<invoke_result_t<F&, T, iter_reference_t<I>>>;

template<typename T, typename I, typename F>
concept left_foldable = requires(F& function, T initial,
                                 left_value_t<T, I, F>& accumulated,
                                 iter_reference_t<I> element) {
    requires movable<F>;
    requires movable<left_value_t<T, I, F>>;
    requires convertible_to<T, left_value_t<T, I, F>>;
    { std::invoke(function, std::move(initial), element) } ->
        convertible_to<left_value_t<T, I, F>>;
    { std::invoke(function, std::move(accumulated), element) } ->
        convertible_to<left_value_t<T, I, F>>;
};

template<typename I, typename F>
using first_value_t = decay_t<invoke_result_t<F&, iter_value_t<I>,
                                               iter_reference_t<I>>>;

template<typename I, typename F>
concept first_foldable = requires(F& function,
                                  iter_value_t<I> first_value,
                                  first_value_t<I, F>& accumulated,
                                  iter_reference_t<I> element) {
    requires movable<F>;
    requires movable<first_value_t<I, F>>;
    requires convertible_to<iter_value_t<I>, first_value_t<I, F>>;
    { std::invoke(function, std::move(first_value), element) } ->
        convertible_to<first_value_t<I, F>>;
    { std::invoke(function, std::move(accumulated), element) } ->
        convertible_to<first_value_t<I, F>>;
};

template<typename T, typename I, typename F>
using right_value_t = decay_t<invoke_result_t<F&, iter_reference_t<I>, T>>;

template<typename T, typename I, typename F>
concept right_foldable = requires(F& function, T initial,
                                  right_value_t<T, I, F>& accumulated,
                                  iter_reference_t<I> element) {
    requires movable<F>;
    requires movable<right_value_t<T, I, F>>;
    requires convertible_to<T, right_value_t<T, I, F>>;
    { std::invoke(function, element, std::move(initial)) } ->
        convertible_to<right_value_t<T, I, F>>;
    { std::invoke(function, element, std::move(accumulated)) } ->
        convertible_to<right_value_t<T, I, F>>;
};

template<typename I, typename F>
using right_first_value_t = decay_t<invoke_result_t<F&, iter_reference_t<I>,
                                                     iter_value_t<I>>>;

template<typename I, typename F>
concept right_first_foldable = requires(F& function,
                                        iter_reference_t<I> element,
                                        iter_value_t<I> last_value,
                                        right_first_value_t<I, F>& accumulated) {
    requires movable<F>;
    requires movable<right_first_value_t<I, F>>;
    requires convertible_to<iter_value_t<I>, right_first_value_t<I, F>>;
    { std::invoke(function, element, std::move(last_value)) } ->
        convertible_to<right_first_value_t<I, F>>;
    { std::invoke(function, element, std::move(accumulated)) } ->
        convertible_to<right_first_value_t<I, F>>;
};

} /* namespace numeric_fold_detail */

template<input_iterator I, sentinel_for<I> S, typename T, typename F>
requires numeric_fold_detail::left_foldable<T, I, F>
constexpr auto fold_left_impl(I first, S last, T init, F function)
    -> numeric_fold_detail::left_value_t<T, I, F> {
    using result_type = numeric_fold_detail::left_value_t<T, I, F>;
    result_type value = static_cast<result_type>(std::move(init));
    for (; first != last; ++first) {
        value = static_cast<result_type>(
            std::invoke(function, std::move(value), *first));
    }
    return value;
}

template<input_range R, typename T, typename F>
requires numeric_fold_detail::left_foldable<T, iterator_t<R>, F>
constexpr auto fold_left_impl(R&& range, T init, F function)
    -> numeric_fold_detail::left_value_t<T, iterator_t<R>, F> {
    return fold_left_impl(ranges::begin(range), ranges::end(range),
                          std::move(init), std::move(function));
}

template<input_iterator I, sentinel_for<I> S, typename T, typename F>
requires numeric_fold_detail::left_foldable<T, I, F>
constexpr auto fold_left_with_iter_impl(I first, S last, T init, F function)
    -> fold_left_with_iter_result<I,
        numeric_fold_detail::left_value_t<T, I, F>> {
    using result_type = numeric_fold_detail::left_value_t<T, I, F>;
    result_type value = static_cast<result_type>(std::move(init));
    for (; first != last; ++first) {
        value = static_cast<result_type>(
            std::invoke(function, std::move(value), *first));
    }
    return {std::move(first), std::move(value)};
}

template<input_range R, typename T, typename F>
requires numeric_fold_detail::left_foldable<T, iterator_t<R>, F>
constexpr auto fold_left_with_iter_impl(R&& range, T init, F function)
    -> fold_left_with_iter_result<borrowed_iterator_t<R>,
        numeric_fold_detail::left_value_t<T, iterator_t<R>, F>> {
    auto result = fold_left_with_iter_impl(ranges::begin(range), ranges::end(range),
                                           std::move(init), std::move(function));
    if constexpr (borrowed_range<R>) {
        return {std::move(result.in), std::move(result.value)};
    } else {
        return {dangling{}, std::move(result.value)};
    }
}

template<input_iterator I, sentinel_for<I> S, typename F>
requires numeric_fold_detail::first_foldable<I, F>
constexpr auto fold_left_first_impl(I first, S last, F function)
    -> optional<numeric_fold_detail::first_value_t<I, F>> {
    using result_type = numeric_fold_detail::first_value_t<I, F>;
    if (first == last) return nullopt;
    result_type value = static_cast<result_type>(*first++);
    for (; first != last; ++first) {
        value = static_cast<result_type>(
            std::invoke(function, std::move(value), *first));
    }
    return optional<result_type>(std::move(value));
}

template<input_range R, typename F>
requires numeric_fold_detail::first_foldable<iterator_t<R>, F>
constexpr auto fold_left_first_impl(R&& range, F function)
    -> optional<numeric_fold_detail::first_value_t<iterator_t<R>, F>> {
    return fold_left_first_impl(ranges::begin(range), ranges::end(range),
                                std::move(function));
}

template<input_iterator I, sentinel_for<I> S, typename F>
requires numeric_fold_detail::first_foldable<I, F>
constexpr auto fold_left_first_with_iter_impl(I first, S last, F function)
    -> fold_left_first_with_iter_result<I,
        numeric_fold_detail::first_value_t<I, F>> {
    using result_type = numeric_fold_detail::first_value_t<I, F>;
    if (first == last) return {std::move(first), nullopt};
    result_type value = static_cast<result_type>(*first++);
    for (; first != last; ++first) {
        value = static_cast<result_type>(
            std::invoke(function, std::move(value), *first));
    }
    return {std::move(first), optional<result_type>(std::move(value))};
}

template<input_range R, typename F>
requires numeric_fold_detail::first_foldable<iterator_t<R>, F>
constexpr auto fold_left_first_with_iter_impl(R&& range, F function)
    -> fold_left_first_with_iter_result<borrowed_iterator_t<R>,
        numeric_fold_detail::first_value_t<iterator_t<R>, F>> {
    auto result = fold_left_first_with_iter_impl(ranges::begin(range),
                                                 ranges::end(range),
                                                 std::move(function));
    if constexpr (borrowed_range<R>) {
        return {std::move(result.in), std::move(result.value)};
    } else {
        return {dangling{}, std::move(result.value)};
    }
}

template<bidirectional_iterator I, typename T, typename F>
requires numeric_fold_detail::right_foldable<T, I, F>
constexpr auto fold_right_impl(I first, I last, T init, F function)
    -> numeric_fold_detail::right_value_t<T, I, F> {
    using result_type = numeric_fold_detail::right_value_t<T, I, F>;
    result_type value = static_cast<result_type>(std::move(init));
    while (first != last) {
        --last;
        value = static_cast<result_type>(
            std::invoke(function, *last, std::move(value)));
    }
    return value;
}

template<bidirectional_range R, typename T, typename F>
requires same_as<iterator_t<R>, sentinel_t<R>> &&
         numeric_fold_detail::right_foldable<T, iterator_t<R>, F>
constexpr auto fold_right_impl(R&& range, T init, F function)
    -> numeric_fold_detail::right_value_t<T, iterator_t<R>, F> {
    return fold_right_impl(ranges::begin(range), ranges::end(range),
                           std::move(init), std::move(function));
}

template<bidirectional_iterator I, typename F>
requires numeric_fold_detail::right_first_foldable<I, F>
constexpr auto fold_right_last_impl(I first, I last, F function)
    -> optional<numeric_fold_detail::right_first_value_t<I, F>> {
    using result_type = numeric_fold_detail::right_first_value_t<I, F>;
    if (first == last) return nullopt;
    --last;
    result_type value = static_cast<result_type>(*last);
    while (first != last) {
        --last;
        value = static_cast<result_type>(
            std::invoke(function, *last, std::move(value)));
    }
    return optional<result_type>(std::move(value));
}

template<bidirectional_range R, typename F>
requires same_as<iterator_t<R>, sentinel_t<R>> &&
         numeric_fold_detail::right_first_foldable<iterator_t<R>, F>
constexpr auto fold_right_last_impl(R&& range, F function)
    -> optional<numeric_fold_detail::right_first_value_t<iterator_t<R>, F>> {
    return fold_right_last_impl(ranges::begin(range), ranges::end(range),
                                std::move(function));
}

/* C++23 fold algorithms are customization-point objects.  Keep the
 * constrained iterator/range implementations above private to this header
 * surface and expose forwarding call operators with the standard names. */
struct fold_left_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_left_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_left_impl(std::forward<Args>(args)...)) {
        return fold_left_impl(std::forward<Args>(args)...);
    }
};

struct fold_left_with_iter_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_left_with_iter_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_left_with_iter_impl(std::forward<Args>(args)...)) {
        return fold_left_with_iter_impl(std::forward<Args>(args)...);
    }
};

struct fold_left_first_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_left_first_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_left_first_impl(std::forward<Args>(args)...)) {
        return fold_left_first_impl(std::forward<Args>(args)...);
    }
};

struct fold_left_first_with_iter_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_left_first_with_iter_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_left_first_with_iter_impl(std::forward<Args>(args)...)) {
        return fold_left_first_with_iter_impl(std::forward<Args>(args)...);
    }
};

struct fold_right_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_right_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_right_impl(std::forward<Args>(args)...)) {
        return fold_right_impl(std::forward<Args>(args)...);
    }
};

struct fold_right_last_fn {
    template<typename... Args>
    requires requires(Args&&... args) {
        fold_right_last_impl(std::forward<Args>(args)...);
    }
    constexpr auto operator()(Args&&... args) const
        -> decltype(fold_right_last_impl(std::forward<Args>(args)...)) {
        return fold_right_last_impl(std::forward<Args>(args)...);
    }
};

inline constexpr fold_left_fn fold_left{};
inline constexpr fold_left_with_iter_fn fold_left_with_iter{};
inline constexpr fold_left_first_fn fold_left_first{};
inline constexpr fold_left_first_with_iter_fn fold_left_first_with_iter{};
inline constexpr fold_right_fn fold_right{};
inline constexpr fold_right_last_fn fold_right_last{};

} /* namespace ranges */
#endif

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_NUMERIC)
/* The installed compilers still report a C++2b language value.  Keep this
 * C++26 facility behind an explicit RinOS preview opt-in until the final mode
 * is available, rather than exposing it in an earlier standard mode. */
namespace numeric_detail {

template<typename T>
inline constexpr bool saturation_integer =
    is_same<remove_cv_t<T>, signed char>::value ||
    is_same<remove_cv_t<T>, unsigned char>::value ||
    is_same<remove_cv_t<T>, short>::value ||
    is_same<remove_cv_t<T>, unsigned short>::value ||
    is_same<remove_cv_t<T>, int>::value ||
    is_same<remove_cv_t<T>, unsigned int>::value ||
    is_same<remove_cv_t<T>, long>::value ||
    is_same<remove_cv_t<T>, unsigned long>::value ||
    is_same<remove_cv_t<T>, long long>::value ||
    is_same<remove_cv_t<T>, unsigned long long>::value;

template<typename T>
[[noreturn]] inline void saturation_divide_by_zero() noexcept {
    __builtin_trap();
}

} /* namespace numeric_detail */

template<typename T,
         enable_if_t<numeric_detail::saturation_integer<T>, int> = 0>
constexpr T saturating_add(T x, T y) noexcept {
    if constexpr (is_unsigned<T>::value) {
        if (x > numeric_limits<T>::max() - y) return numeric_limits<T>::max();
    } else {
        if (y > 0 && x > numeric_limits<T>::max() - y) {
            return numeric_limits<T>::max();
        }
        if (y < 0 && x < numeric_limits<T>::min() - y) {
            return numeric_limits<T>::min();
        }
    }
    return static_cast<T>(x + y);
}

template<typename T,
         enable_if_t<numeric_detail::saturation_integer<T>, int> = 0>
constexpr T saturating_sub(T x, T y) noexcept {
    if constexpr (is_unsigned<T>::value) {
        if (x < y) return T(0);
    } else {
        if (y > 0 && x < numeric_limits<T>::min() + y) {
            return numeric_limits<T>::min();
        }
        if (y < 0 && x > numeric_limits<T>::max() + y) {
            return numeric_limits<T>::max();
        }
    }
    return static_cast<T>(x - y);
}

template<typename T,
         enable_if_t<numeric_detail::saturation_integer<T>, int> = 0>
constexpr T saturating_mul(T x, T y) noexcept {
    if (x == 0 || y == 0) return T(0);

    if constexpr (is_unsigned<T>::value) {
        if (x > numeric_limits<T>::max() / y) return numeric_limits<T>::max();
    } else if (x > 0) {
        if (y > 0 && x > numeric_limits<T>::max() / y) {
            return numeric_limits<T>::max();
        }
        if (y < 0 && y < numeric_limits<T>::min() / x) {
            return numeric_limits<T>::min();
        }
    } else {
        if (y > 0 && x < numeric_limits<T>::min() / y) {
            return numeric_limits<T>::min();
        }
        if (y < 0 && x < numeric_limits<T>::max() / y) {
            return numeric_limits<T>::max();
        }
    }
    return static_cast<T>(x * y);
}

template<typename T,
         enable_if_t<numeric_detail::saturation_integer<T>, int> = 0>
constexpr T saturating_div(T x, T y) noexcept {
    if (y == 0) numeric_detail::saturation_divide_by_zero<T>();
    if constexpr (is_signed<T>::value) {
        if (x == numeric_limits<T>::min() && y == T(-1)) {
            return numeric_limits<T>::max();
        }
    }
    return static_cast<T>(x / y);
}

template<typename R, typename T,
         enable_if_t<numeric_detail::saturation_integer<R> &&
                         numeric_detail::saturation_integer<T>,
                     int> = 0>
constexpr R saturating_cast(T x) noexcept {
    if constexpr (is_signed<T>::value) {
        if constexpr (is_signed<R>::value) {
            if constexpr (numeric_limits<R>::digits < numeric_limits<T>::digits) {
                if (x > static_cast<T>(numeric_limits<R>::max())) {
                    return numeric_limits<R>::max();
                }
                if (x < static_cast<T>(numeric_limits<R>::min())) {
                    return numeric_limits<R>::min();
                }
            }
        } else {
            if (x < 0) return R(0);
            using unsigned_source = make_unsigned_t<T>;
            const unsigned_source value = static_cast<unsigned_source>(x);
            if constexpr (numeric_limits<R>::digits <
                          numeric_limits<unsigned_source>::digits) {
                if (value > static_cast<unsigned_source>(numeric_limits<R>::max())) {
                    return numeric_limits<R>::max();
                }
            }
        }
    } else if constexpr (is_signed<R>::value) {
        if constexpr (numeric_limits<R>::digits < numeric_limits<T>::digits) {
            if (x > static_cast<T>(numeric_limits<R>::max())) {
                return numeric_limits<R>::max();
            }
        }
    } else if constexpr (numeric_limits<R>::digits < numeric_limits<T>::digits) {
        if (x > static_cast<T>(numeric_limits<R>::max())) {
            return numeric_limits<R>::max();
        }
    }
    return static_cast<R>(x);
}
#endif

#if (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_NUMERIC)) && \
    (__cplusplus >= 202002L)
/* C++26 saturating arithmetic.  Reuse the checked single-type owners above
 * after converting both operands to their common integer type. */
#define __cpp_lib_saturation_arithmetic 202311L
#define __cpp_lib_stdckdint_h 202603L

namespace saturation_detail {

template<typename T>
struct is_saturation_integer
    : integral_constant<bool,
        numeric_detail::saturation_integer<T> &&
        !is_same<remove_cv_t<T>, bool>::value> {};

template<typename T, typename U>
using result_t = common_type_t<T, U>;

template<typename T, typename U>
using enabled = integral_constant<bool,
    is_saturation_integer<T>::value && is_saturation_integer<U>::value>;

} /* namespace saturation_detail */

template<typename T, typename U,
         enable_if_t<saturation_detail::enabled<T, U>::value, int> = 0>
constexpr saturation_detail::result_t<T, U> add_sat(T lhs, U rhs) noexcept {
    using result_type = saturation_detail::result_t<T, U>;
    return saturating_add(static_cast<result_type>(lhs),
                          static_cast<result_type>(rhs));
}

template<typename T, typename U,
         enable_if_t<saturation_detail::enabled<T, U>::value, int> = 0>
constexpr saturation_detail::result_t<T, U> sub_sat(T lhs, U rhs) noexcept {
    using result_type = saturation_detail::result_t<T, U>;
    return saturating_sub(static_cast<result_type>(lhs),
                          static_cast<result_type>(rhs));
}

template<typename T, typename U,
         enable_if_t<saturation_detail::enabled<T, U>::value, int> = 0>
constexpr saturation_detail::result_t<T, U> mul_sat(T lhs, U rhs) noexcept {
    using result_type = saturation_detail::result_t<T, U>;
    return saturating_mul(static_cast<result_type>(lhs),
                          static_cast<result_type>(rhs));
}

template<typename T, typename U,
         enable_if_t<saturation_detail::enabled<T, U>::value, int> = 0>
constexpr saturation_detail::result_t<T, U> div_sat(T lhs, U rhs) noexcept {
    using result_type = saturation_detail::result_t<T, U>;
    return saturating_div(static_cast<result_type>(lhs),
                          static_cast<result_type>(rhs));
}

/* C++26's standard spelling for the checked narrowing operation.  Keep the
 * older RinOS `saturating_cast` helper as an implementation owner and expose
 * the standard alias under the same integer-only constraints. */
template<typename R, typename T,
         enable_if_t<saturation_detail::enabled<R, T>::value, int> = 0>
constexpr R saturate_cast(T value) noexcept {
    return saturating_cast<R>(value);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * gcd, lcm (C++17)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 201703L
namespace numeric_detail {

/* C++17 specifies gcd/lcm behavior as undefined when an absolute input or
 * the result cannot be represented by the common type.  RinOS keeps that
 * contract deterministic: detect the condition before a narrowing cast or
 * unsigned product can silently wrap, then stop without publishing a value.
 * The constexpr return type keeps valid calls usable in constant evaluation;
 * an invalid constant expression is rejected by the compiler. */
template<typename Common>
[[noreturn]] constexpr Common arithmetic_contract_fail() noexcept {
    __builtin_trap();
}

template<typename Unsigned, typename Value>
constexpr Unsigned magnitude(Value value) noexcept {
    const Unsigned converted = static_cast<Unsigned>(value);
    if constexpr (is_signed<Value>::value) {
        if (value < 0) return Unsigned(0) - converted;
    }
    return converted;
}

template<typename Float>
constexpr Float absolute(Float value) noexcept {
    return value < Float(0) ? -value : value;
}

} /* namespace numeric_detail */

template<typename M, typename N,
         enable_if_t<
             is_integral<M>::value && is_integral<N>::value &&
             !is_same<remove_cv_t<M>, bool>::value &&
             !is_same<remove_cv_t<N>, bool>::value,
             int> = 0>
constexpr common_type_t<M, N> gcd(M m, N n) noexcept {
    using Common = common_type_t<M, N>;
    using Unsigned = make_unsigned_t<Common>;
    Unsigned a = numeric_detail::magnitude<Unsigned>(m);
    Unsigned b = numeric_detail::magnitude<Unsigned>(n);
    if constexpr (is_signed<Common>::value) {
        if (a > static_cast<Unsigned>(numeric_limits<Common>::max()) ||
            b > static_cast<Unsigned>(numeric_limits<Common>::max())) {
            return numeric_detail::arithmetic_contract_fail<Common>();
        }
    }
    while (b != 0) {
        const Unsigned t = b;
        b = a % b;
        a = t;
    }
    return static_cast<Common>(a);
}

template<typename M, typename N,
         enable_if_t<
             is_integral<M>::value && is_integral<N>::value &&
             !is_same<remove_cv_t<M>, bool>::value &&
             !is_same<remove_cv_t<N>, bool>::value,
             int> = 0>
constexpr common_type_t<M, N> lcm(M m, N n) noexcept {
    using Common = common_type_t<M, N>;
    using Unsigned = make_unsigned_t<Common>;
    if (m == 0 || n == 0) return Common(0);
    const Unsigned left = numeric_detail::magnitude<Unsigned>(m);
    const Unsigned right = numeric_detail::magnitude<Unsigned>(n);
    const Unsigned divisor = static_cast<Unsigned>(gcd(m, n));
    const Unsigned maximum = static_cast<Unsigned>(
        numeric_limits<Common>::max());
    const Unsigned reduced_left = left / divisor;
    if (right != 0 && reduced_left > maximum / right) {
        return numeric_detail::arithmetic_contract_fail<Common>();
    }
    return static_cast<Common>(reduced_left * right);
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * midpoint (C++20)
 * ═══════════════════════════════════════════════════════════════*/

#if __cplusplus >= 202002L
template<typename T,
         enable_if_t<is_integral<T>::value &&
                     !is_same<remove_cv_t<T>, bool>::value,
                     int> = 0>
constexpr T midpoint(T a, T b) noexcept {
    using Unsigned = make_unsigned_t<T>;
    if (a < b) {
        return static_cast<T>(a + static_cast<T>(
            (static_cast<Unsigned>(b) - static_cast<Unsigned>(a)) / 2u));
    }
    return static_cast<T>(a - static_cast<T>(
        (static_cast<Unsigned>(a) - static_cast<Unsigned>(b)) / 2u));
}

template<typename T,
         enable_if_t<is_floating_point<T>::value, int> = 0>
constexpr T midpoint(T a, T b) noexcept {
    const T low = numeric_limits<T>::min() * T(2);
    const T high = numeric_limits<T>::max() / T(2);
    const T abs_a = numeric_detail::absolute(a);
    const T abs_b = numeric_detail::absolute(b);
    if (abs_a <= high && abs_b <= high) return (a + b) / T(2);
    if (abs_a < low) return a + b / T(2);
    if (abs_b < low) return a / T(2) + b;
    return a / T(2) + b / T(2);
}

template<typename T>
constexpr T* midpoint(T* a, T* b) noexcept {
    return a + (b - a) / 2;
}
#endif

/* plus, minus, multiplies, etc. は functional.h で定義済み */

} /* namespace std */

#undef RIN_NUMERIC_CONSTEXPR20

#endif /* __cplusplus */
#endif /* RINCXX_NUMERIC_H */
