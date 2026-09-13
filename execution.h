/*
 * RinOS C++ <execution> ✿
 * Execution-policy tags and policy classification
 */

#ifndef RINCXX_EXECUTION_H
#define RINCXX_EXECUTION_H

#include "rincxx.h"
#include "type_traits.h"
#include "version.h"

#if __cplusplus >= 201703L

namespace std {
namespace execution {

struct sequenced_policy {};
struct parallel_policy {};
struct parallel_unsequenced_policy {};
#if __cplusplus >= 202002L
struct unsequenced_policy {};
#endif

inline constexpr sequenced_policy seq{};
inline constexpr parallel_policy par{};
inline constexpr parallel_unsequenced_policy par_unseq{};
#if __cplusplus >= 202002L
inline constexpr unsequenced_policy unseq{};
#endif

} /* namespace execution */

template<typename T>
struct is_execution_policy : false_type {};

template<>
struct is_execution_policy<execution::sequenced_policy> : true_type {};

template<>
struct is_execution_policy<execution::parallel_policy> : true_type {};

template<>
struct is_execution_policy<execution::parallel_unsequenced_policy> : true_type {};

#if __cplusplus >= 202002L
template<>
struct is_execution_policy<execution::unsequenced_policy> : true_type {};
#endif

/* C++17 exposes the variable-template spelling alongside the class trait.
 * Keep it in this header (and therefore available through <version>) so
 * policy-constrained algorithms and users observe one capability owner. */
template<typename T>
constexpr bool is_execution_policy_v = is_execution_policy<T>::value;

} /* namespace std */

#endif /* C++17 */

#endif /* RINCXX_EXECUTION_H */
