/*
 * RinOS libcxx object-pointer total-order primitive
 */

#ifndef RINCXX_POINTER_ORDER_H
#define RINCXX_POINTER_ORDER_H

#include "rincxx.h"
#include "type_traits.h"
#include "cstdint.h"

#ifdef __cplusplus
namespace std {
namespace detail {

template<typename T>
struct is_object_pointer
    : integral_constant<bool,
          is_pointer<T>::value &&
          !is_function<typename remove_pointer<T>::type>::value> {};

template<typename T>
struct is_function_pointer
    : integral_constant<bool,
          is_pointer<T>::value &&
          is_function<typename remove_pointer<T>::type>::value> {};

/* RinOS targets use a flat, byte-addressed object-pointer ABI. Convert both
 * object pointers to the common cv-void representation before forming the
 * documented implementation-defined total order. Function pointers remain
 * outside this helper because their representation need not be comparable to
 * object addresses. */
template<typename T, typename U>
inline bool object_pointer_equal(T left, U right) noexcept {
    return static_cast<const volatile void*>(left) ==
           static_cast<const volatile void*>(right);
}

template<typename T, typename U>
inline bool object_pointer_total_less(T left, U right) noexcept {
    if (object_pointer_equal(left, right)) return false;
    return reinterpret_cast<uintptr_t>(
               static_cast<const volatile void*>(left)) <
           reinterpret_cast<uintptr_t>(
               static_cast<const volatile void*>(right));
}

/* Hash the same flat object-pointer representation used by the owner.  This
 * is deliberately separate from the ordering predicate so callers do not
 * grow ad-hoc pointer-to-integer casts that disagree about cv qualification
 * or the common void representation. */
template<typename T>
inline uintptr_t object_pointer_hash(T pointer) noexcept {
    static_assert(is_object_pointer<T>::value, "object pointer required");
    return reinterpret_cast<uintptr_t>(
        static_cast<const volatile void*>(pointer));
}

/* RinOS also uses a flat function-pointer ABI on every supported x86 target.
 * The C++ relational operators are not defined as a portable function-pointer
 * total order and Clang 21 diagnoses their use.  Convert the representation to
 * uintptr_t instead, just as the documented object-pointer order does above.
 * Keeping this primitive separate prevents accidental object/function mixing. */
template<typename T>
inline uintptr_t function_pointer_order_key(T pointer) noexcept {
    static_assert(is_function_pointer<T>::value,
                  "function pointer required");
    static_assert(sizeof(T) <= sizeof(uintptr_t),
                  "RinOS function pointer must fit uintptr_t");
    return reinterpret_cast<uintptr_t>(pointer);
}

template<typename T>
inline uintptr_t function_pointer_hash(T pointer) noexcept {
    static_assert(is_function_pointer<T>::value, "function pointer required");
    return function_pointer_order_key(pointer);
}

template<typename T, typename U>
inline bool function_pointer_equal(T left, U right) noexcept {
    static_assert(is_function_pointer<T>::value &&
                  is_function_pointer<U>::value,
                  "function pointers required");
    return function_pointer_order_key(left) == function_pointer_order_key(right);
}

template<typename T, typename U>
inline bool function_pointer_total_less(T left, U right) noexcept {
    static_assert(is_function_pointer<T>::value &&
                  is_function_pointer<U>::value,
                  "function pointers required");
    return function_pointer_order_key(left) < function_pointer_order_key(right);
}

} /* namespace detail */
} /* namespace std */
#endif

#endif /* RINCXX_POINTER_ORDER_H */
