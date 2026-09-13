/*
 * RinOS C++ <stdckdint.h>
 * C++26 checked integer operations
 */

#ifndef RINCXX_STDCKDINT_H
#define RINCXX_STDCKDINT_H

#ifdef __cplusplus

#include "rincxx.h"
#include "type_traits.h"

/* GCC 13 still reports the provisional C++2b language value.  Do not expose
 * the C++26 C-compatibility header before its final language mode unless a
 * caller explicitly opts in to this RinOS preview. */
#if (__cplusplus > 202302L || defined(RIN_ENABLE_CXX26_NUMERIC)) && \
    (defined(__GNUC__) || defined(__clang__))

#define __STDC_VERSION_STDCKDINT_H__ 202311L

namespace std {
namespace checked_integer_detail {

template<typename T>
inline constexpr bool standard_signed_or_unsigned_integer =
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

template<typename Result>
inline bool checked_result_pointer(Result* result) {
    if (result == nullptr) __builtin_trap();
    return false;
}

} /* namespace checked_integer_detail */

template<typename type1, typename type2, typename type3,
         enable_if_t<
             checked_integer_detail::standard_signed_or_unsigned_integer<type1> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type2> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type3> &&
             is_same<type1, remove_cv_t<type1>>::value,
             int> = 0>
bool ckd_add(type1* result, type2 a, type3 b) {
    type1 candidate{};
    const bool overflow = __builtin_add_overflow(a, b, &candidate);
    checked_integer_detail::checked_result_pointer(result);
    *result = candidate;
    return overflow;
}

template<typename type1, typename type2, typename type3,
         enable_if_t<
             checked_integer_detail::standard_signed_or_unsigned_integer<type1> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type2> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type3> &&
             is_same<type1, remove_cv_t<type1>>::value,
             int> = 0>
bool ckd_sub(type1* result, type2 a, type3 b) {
    type1 candidate{};
    const bool overflow = __builtin_sub_overflow(a, b, &candidate);
    checked_integer_detail::checked_result_pointer(result);
    *result = candidate;
    return overflow;
}

template<typename type1, typename type2, typename type3,
         enable_if_t<
             checked_integer_detail::standard_signed_or_unsigned_integer<type1> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type2> &&
             checked_integer_detail::standard_signed_or_unsigned_integer<type3> &&
             is_same<type1, remove_cv_t<type1>>::value,
             int> = 0>
bool ckd_mul(type1* result, type2 a, type3 b) {
    type1 candidate{};
    const bool overflow = __builtin_mul_overflow(a, b, &candidate);
    checked_integer_detail::checked_result_pointer(result);
    *result = candidate;
    return overflow;
}

} /* namespace std */

/* The C compatibility header publishes the checked-operation names in the
 * global namespace. Keep the C++ std:: entry points as the implementation
 * owner, but make direct <stdckdint.h> callers observe the required names too
 * instead of requiring an unrelated <numeric> using-declaration. */
using std::ckd_add;
using std::ckd_sub;
using std::ckd_mul;
#endif /* C++ preview gate */
#else

/* C23 exposes these operations as type-generic macros. The preview gate is
 * explicit on older C frontends so an ordinary C11 include does not advertise
 * a facility that the selected language mode does not provide. */
#if (__STDC_VERSION__ >= 202311L || defined(RIN_ENABLE_CXX26_NUMERIC)) && \
    (defined(__GNUC__) || defined(__clang__))
#define __STDC_VERSION_STDCKDINT_H__ 202311L
#define ckd_add(result, a, b) __builtin_add_overflow((a), (b), (result))
#define ckd_sub(result, a, b) __builtin_sub_overflow((a), (b), (result))
#define ckd_mul(result, a, b) __builtin_mul_overflow((a), (b), (result))
#endif
#endif

#endif /* RINCXX_STDCKDINT_H */
