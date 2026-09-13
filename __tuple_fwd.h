/*
 * RinOS C++ tuple forward declarations
 * Internal header - PURE FORWARD DECLARATIONS ONLY
 *
 * 重要: このヘッダには cv-forwarding 特殊化を置かない
 * cv-forwarding は tuple.h / utility.h など本体定義が揃う場所で行う
 *
 * これにより、aggregate 型（BackingArrayPtrs など）の structured binding が
 * tuple-like 判定に引っかからず、正しく aggregate 分解される
 */

#ifndef RINCXX_TUPLE_FWD_H
#define RINCXX_TUPLE_FWD_H

#include "type_traits.h"

namespace std {

template<class T, size_t N>
struct array;

namespace ranges {
template<class I, class S>
class subrange;
}

// =============================================================================
// tuple_size - PRIMARY TEMPLATE ONLY
// =============================================================================

// Primary template: intentionally incomplete
// Specializations are provided in tuple.h, utility.h, array.h for their types
template<class T>
struct tuple_size;

// Array specialization (required by standard, safe to put here)
template<class T, size_t N>
struct tuple_size<T[N]> : integral_constant<size_t, N> {};

// Unknown bound arrays: intentionally incomplete
template<class T>
struct tuple_size<T[]>;

// NOTE: cv-qualified forwarding (tuple_size<const T>, etc.) is NOT here
// It's defined in tuple.h and utility.h where tuple/pair are complete


// =============================================================================
// tuple_element - PRIMARY TEMPLATE ONLY
// =============================================================================

// Primary template: intentionally incomplete
template<size_t I, class T>
struct tuple_element;

// Array specialization
template<size_t I, class T, size_t N>
struct tuple_element<I, T[N]> {
    static_assert(I < N, "tuple_element index out of bounds for array");
    using type = T;
};

// Unknown bound arrays: intentionally incomplete
template<size_t I, class T>
struct tuple_element<I, T[]>;

// NOTE: cv-qualified forwarding is NOT here
// It's defined in tuple.h and utility.h


// =============================================================================
// tuple_element_t - alias template (safe, doesn't instantiate anything)
// =============================================================================

template<size_t I, class T>
using tuple_element_t = typename tuple_element<I, T>::type;

} // namespace std

#endif /* RINCXX_TUPLE_FWD_H */
