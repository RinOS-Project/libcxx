/*
 * RinOS C++ <version>
 * C++20 Feature test macros
 */

#ifndef RINCXX_VERSION_H
#define RINCXX_VERSION_H

/* Language feature-test macros (`__cpp_*`) describe compiler support, not
 * library support.  Do not manufacture them: users must see the values from
 * their selected compiler language mode. */

/* This is a capability registry, not a wish list.  A feature-test macro is
 * present only after the matching Rin-owned surface has a positive contract
 * test.  Partial facilities deliberately remain available as extensions in
 * their own headers without making a portable availability promise here. */

/* GCC 13 still reports the C++23 working-draft value (202100L) for -std=c++2b.
 * It is nevertheless the language mode in which the implemented C++23
 * library facilities are enabled. */
#if defined(__cplusplus) && __cplusplus > 202002L
#define RIN_LIBCXX_CXX23_DIALECT 1
#endif

/* bit_cast has no truthful fallback: only compiler builtins that have been
 * exercised by the Rin tests may publish it. */
#if defined(__cplusplus) && __cplusplus >= 202002L
#if defined(__clang__)
#if __has_builtin(__builtin_bit_cast)
#define RIN_LIBCXX_HAS_BIT_CAST_BACKEND 1
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11
#define RIN_LIBCXX_HAS_BIT_CAST_BACKEND 1
#elif defined(_MSC_VER) && _MSC_VER >= 1928 && \
    (defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64))
#define RIN_LIBCXX_HAS_BIT_CAST_BACKEND 1
#endif

/* The remaining <bit> primitives have a constexpr generic implementation.
 * MSVC's supported targets are little-endian; the GNU/Clang path uses the
 * compiler byte-order macros. */
#if defined(__clang__) || (defined(__GNUC__) && !defined(__clang__)) || \
    (defined(_MSC_VER) && _MSC_VER >= 1928 && \
     (defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64)))
#define RIN_LIBCXX_HAS_BIT_OPERATIONS_BACKEND 1
#endif

/* source_location is backend-specific.  GCC 11 and newer expose the
 * file/function/line call-site builtins.  Keep their standard column-zero
 * sentinel stable instead of inferring a column rule from later extensions;
 * Clang uses its individually probed call-site builtins. */
#if defined(__clang__)
#if __has_builtin(__builtin_FILE) && __has_builtin(__builtin_FUNCTION) && \
    __has_builtin(__builtin_LINE)
#define RIN_LIBCXX_HAS_SOURCE_LOCATION_BACKEND 1
#if __has_builtin(__builtin_COLUMN)
#define RIN_LIBCXX_SOURCE_LOCATION_HAS_COLUMN 1
#else
#define RIN_LIBCXX_SOURCE_LOCATION_HAS_COLUMN 0
#endif
#endif
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11
#define RIN_LIBCXX_HAS_SOURCE_LOCATION_BACKEND 1
#define RIN_LIBCXX_SOURCE_LOCATION_HAS_COLUMN 0
#elif defined(_MSC_VER) && _MSC_VER >= 1928 && \
    (defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64))
#define RIN_LIBCXX_HAS_SOURCE_LOCATION_BACKEND 1
#define RIN_LIBCXX_SOURCE_LOCATION_HAS_COLUMN 1
#endif
#endif

/* C++17 node extraction is provided by all four associative containers. */
#if defined(__cplusplus) && __cplusplus >= 201703L
#define __cpp_lib_node_extract 201606L
#endif

/* ===================================================================
 * Library feature test macros - C++20
 * ===================================================================*/

#if defined(__cplusplus) && __cplusplus >= 202002L
#define __cpp_lib_destroying_delete          201806L
#define __cpp_lib_chrono                    201907L
#define __cpp_lib_constexpr_char_traits     201811L
#define __cpp_lib_interpolate               201902L
#define __cpp_lib_constexpr_tuple           201811L
#if __cplusplus == 202002L
#define __cpp_lib_optional                  202106L
#endif
#ifndef __cpp_lib_constexpr_string_view
#define __cpp_lib_constexpr_string_view     201811L
#endif
#define __cpp_lib_array_constexpr           201811L
#define __cpp_lib_to_array                  201907L
#if defined(RIN_LIBCXX_HAS_SOURCE_LOCATION_BACKEND)
#define __cpp_lib_source_location           201907L
#endif
#if defined(__cplusplus) && __cplusplus >= 202400L
#define __cpp_lib_span                      202311L
#else
#define __cpp_lib_span                      202002L
#endif
#if defined(RIN_LIBCXX_HAS_BIT_CAST_BACKEND)
#define __cpp_lib_bit_cast                  201806L
#endif
#if defined(RIN_LIBCXX_HAS_BIT_OPERATIONS_BACKEND)
#define __cpp_lib_bitops                    201907L
#define __cpp_lib_endian                    201907L
#endif
#define __cpp_lib_math_constants            201907L
#define __cpp_lib_bind_front                201907L
#define __cpp_lib_latch                     201907L
#define __cpp_lib_barrier                   201907L
#define __cpp_lib_semaphore                 201907L
#define __cpp_lib_jthread                   201911L
#if defined(__cplusplus) && __cplusplus >= 202002L && \
    ((defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L) || \
     (defined(__cpp_coroutines) && __cpp_coroutines >= 201703L))
#define __cpp_lib_coroutine                 201902L
#endif
#define __cpp_lib_atomic_ref                201806L
#define __cpp_lib_atomic_flag_test          201907L
#define __cpp_lib_atomic_wait               201907L
#define __cpp_lib_atomic_float              201711L
#define __cpp_lib_smart_ptr_for_overwrite   202002L
#define __cpp_lib_starts_ends_with          201711L
#define __cpp_lib_integer_comparison_functions 202002L
#define __cpp_lib_erase_if                  202002L
#define __cpp_lib_generic_unordered_lookup  201811L
#define __cpp_lib_bounded_array_traits      201902L
#define __cpp_lib_is_constant_evaluated     201811L
#define __cpp_lib_type_identity             201806L
#define __cpp_lib_remove_cvref              201711L
#define __cpp_lib_ranges                    201911L
#define __cpp_lib_ranges_istream             202202L
#if (defined(__clang__) && __has_builtin(__is_layout_compatible) && \
     __has_builtin(__builtin_is_corresponding_member)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
#define __cpp_lib_is_layout_compatible      201907L
#endif
#if (defined(__clang__) && __has_builtin(__is_pointer_interconvertible_base_of) && \
     __has_builtin(__builtin_is_pointer_interconvertible_with_class)) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12)
#define __cpp_lib_is_pointer_interconvertible 201907L
#endif
#endif

/* ===================================================================
 * Library feature test macros - C++23
 * ===================================================================*/

#if defined(RIN_LIBCXX_CXX23_DIALECT)
#if defined(RIN_LIBCXX_HAS_BIT_CAST_BACKEND)
#define __cpp_lib_byteswap                  202110L
#endif
#define __cpp_lib_bind_back                 202202L
#define __cpp_lib_forward_like                  202207L
#define __cpp_lib_unreachable                   202202L
#define __cpp_lib_invoke_r                  202106L
#define __cpp_lib_move_only_function        202110L
#define __cpp_lib_adaptor_iterator_pair_constructor 202106L
#define __cpp_lib_stacktrace                 202011L
#define __cpp_lib_to_underlying                 202102L
/* All standard containers covered by P1206R7 expose their C++23 range
 * constructors/modifiers in this implementation. */
#define __cpp_lib_containers_ranges          202202L
#define __cpp_lib_tuple_like                 202207L
#define __cpp_lib_string_contains             202011L
#define __cpp_lib_string_resize_and_overwrite 202110L
#define __cpp_lib_ranges_iota                 202202L
#define __cpp_lib_ranges_fold                 202207L
#define __cpp_lib_ranges_contains             202207L
#define __cpp_lib_ranges_starts_ends_with     202106L
#define __cpp_lib_ranges_find_last            202207L
#define __cpp_lib_ranges_zip                  202110L
#define __cpp_lib_ranges_as_const             202207L
#define __cpp_lib_ranges_enumerate             202302L
#define __cpp_lib_ranges_repeat                202207L
#define __cpp_lib_ranges_cartesian_product     202207L
#define __cpp_lib_ranges_to_container           202202L
#define __cpp_lib_ranges_chunk                  202202L
#define __cpp_lib_ranges_slide                  202202L
#define __cpp_lib_ranges_stride                 202207L
#define __cpp_lib_ranges_chunk_by               202202L
#define __cpp_lib_ranges_split                  202207L
#define __cpp_lib_ranges_as_rvalue              202207L
#define __cpp_lib_ranges_join_with              202202L
#define __cpp_lib_ranges_concat                  202403L
#define __cpp_lib_associative_heterogeneous_erasure 202110L
#define __cpp_lib_ranges_join_with              202202L
#define __cpp_lib_optional                    202110L
#define __cpp_lib_is_scoped_enum              202011L
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_RATIO)
#define __cpp_lib_ratio                       202306L
#endif
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_ASSOCIATIVE_INSERTION)
/* P2363 heterogeneous try_emplace/insert_or_assign. */
#define __cpp_lib_associative_heterogeneous_insertion 202311L
#endif
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_NUMERIC)
/* P0543 saturation arithmetic.  The preview gate is shared with
 * <numeric>; do not advertise the API in older modes without the explicit
 * Rin extension opt-in. */
#define __cpp_lib_saturation_arithmetic       202311L
#define __cpp_lib_stdckdint_h                 202603L
#endif
#endif

/* Heterogeneous lookup for ordered associative containers was standardized
 * in C++14.  Publish this independently of the C++17 node-handle block so
 * C++14 consumers can gate find/count/bound operations portably. */
#if defined(__cplusplus) && __cplusplus >= 201402L
#define __cpp_lib_generic_associative_lookup 201304L
#endif

/* ===================================================================
 * Library feature test macros - C++17
 * ===================================================================*/

#if defined(__cplusplus) && __cplusplus >= 201703L
#define __cpp_lib_tuples_by_type             201304L
#define __cpp_lib_tuple_element_t           201402L
#define __cpp_lib_aligned_new                201606L
#define __cpp_lib_hardware_interference_size 201703L
#define __cpp_lib_launder                   201606L
#define __cpp_lib_execution               201603L
#define __cpp_lib_string_view              201803L
#if __cplusplus < 202002L
#define __cpp_lib_chrono                    201611L
#define __cpp_lib_array_constexpr           201603L
#endif
#define __cpp_lib_invoke                    201411L
#define __cpp_lib_any                       201606L
#if __cplusplus < 202002L
/* C++17 optional's engaged/disengaged, value, and comparison surface is
 * implemented and linked-tested. C++23 upgrades this value above when the
 * monadic operations are enabled. */
#define __cpp_lib_optional                  201606L
#endif
#define __cpp_lib_not_fn                    201603L
#define __cpp_lib_default_searcher           201603L
#define __cpp_lib_boyer_moore_searcher        201603L
#define __cpp_lib_is_swappable               201603L
#define __cpp_lib_is_nothrow_convertible     201806L
#define __cpp_lib_bool_constant              201505L
#define __cpp_lib_logical_traits             201510L
#define __cpp_lib_type_trait_variable_templates 201510L
#define __cpp_lib_void_t                     201411L
#define __cpp_lib_is_invocable               201703L
#define __cpp_lib_has_unique_object_representations 201606L
#define __cpp_lib_is_aggregate                201703L
#if __cplusplus >= 202002L
#define __cpp_lib_variant                   202106L
#else
#define __cpp_lib_variant                   202102L
#endif
#define __cpp_lib_apply                     201603L
#define __cpp_lib_make_from_tuple            201606L
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_CHARCONV)
/* P0067R5 is available from C++17; P2497R0 raises the value for the
 * explicit C++26 preview exposed by <charconv>. */
#define __cpp_lib_to_chars                  202306L
#else
#define __cpp_lib_to_chars                  201611L
#endif
#endif

/* ===================================================================
 * Library feature test macros - C++14
 * ===================================================================*/

#if defined(__cplusplus) && __cplusplus >= 201402L
#define __cpp_lib_tuples_by_type             201304L
#define __cpp_lib_tuple_element_t           201402L
#define __cpp_lib_integer_sequence          201304L
#define __cpp_lib_exchange_function         201304L
#endif

/* ===================================================================
 * Library feature test macros - C++11
 * ===================================================================*/

#if defined(__cplusplus) && __cplusplus >= 201103L
#define __cpp_lib_allocator_traits_is_always_equal 201411L
#endif

/* ===================================================================
 * RinOS specific version info
 * ===================================================================*/

#define __RINOS_LIBCXX__                    1
#define __RINOS_LIBCXX_VERSION__            20240101L
#define __RINOS_LIBCXX_VERSION_MAJOR__      1
#define __RINOS_LIBCXX_VERSION_MINOR__      0
#define __RINOS_LIBCXX_VERSION_PATCH__      0

#endif /* RINCXX_VERSION_H */
