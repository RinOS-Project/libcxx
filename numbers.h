/*
 * RinOS C++ <numbers>
 * Mathematical constants (C++20)
 */

#ifndef RINCXX_NUMBERS_H
#define RINCXX_NUMBERS_H

#include "rincxx.h"
#include "version.h"
#include "type_traits.h"
#include "concepts.h"

#ifdef __cplusplus

#if __cplusplus >= 202002L
namespace std {
namespace numbers {

/* ===================================================================
 * Primary templates (require floating-point type)
 * ===================================================================*/

template<typename T> requires is_floating_point_v<T>
inline constexpr T e_v = T(2.718281828459045235360287471352662498L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T log2e_v = T(1.442695040888963407359924681001892137L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T log10e_v = T(0.434294481903251827651128918916605082L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T pi_v = T(3.141592653589793238462643383279502884L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T inv_pi_v = T(0.318309886183790671537767526745028724L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T inv_sqrtpi_v = T(0.564189583547756286948079451560772586L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T ln2_v = T(0.693147180559945309417232121458176568L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T ln10_v = T(2.302585092994045684017991454684364208L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T sqrt2_v = T(1.414213562373095048801688724209698079L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T sqrt3_v = T(1.732050807568877293527446341505872367L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T inv_sqrt3_v = T(0.577350269189625764509148780501957456L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T egamma_v = T(0.577215664901532860606512090082402431L);

template<typename T> requires is_floating_point_v<T>
inline constexpr T phi_v = T(1.618033988749894848204586834365638118L);

/* ===================================================================
 * double specializations (default)
 * ===================================================================*/

inline constexpr double e = e_v<double>;
inline constexpr double log2e = log2e_v<double>;
inline constexpr double log10e = log10e_v<double>;
inline constexpr double pi = pi_v<double>;
inline constexpr double inv_pi = inv_pi_v<double>;
inline constexpr double inv_sqrtpi = inv_sqrtpi_v<double>;
inline constexpr double ln2 = ln2_v<double>;
inline constexpr double ln10 = ln10_v<double>;
inline constexpr double sqrt2 = sqrt2_v<double>;
inline constexpr double sqrt3 = sqrt3_v<double>;
inline constexpr double inv_sqrt3 = inv_sqrt3_v<double>;
inline constexpr double egamma = egamma_v<double>;
inline constexpr double phi = phi_v<double>;

} /* namespace numbers */
} /* namespace std */
#endif /* __cplusplus >= 202002L */

#endif /* __cplusplus */
#endif /* RINCXX_NUMBERS_H */
