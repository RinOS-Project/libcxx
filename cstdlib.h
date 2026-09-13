/* SPDX-License-Identifier: MIT */
/*
 * RinOS C++ <cstdlib>
 * C++ wrapper for stdlib.h - standalone implementation for C++ namespace
 */

#ifndef RINCXX_CSTDLIB_H
#define RINCXX_CSTDLIB_H

#include "../libc/stddef.h"
#include "../libc/stdint.h"

/* A host C++ <stdlib.h>/<cstdlib> included before this header has already
 * populated namespace std.  Adding Rin inline definitions beside it is
 * ill-formed, so that include order deliberately reuses the host facility.
 * Header-first consumers take the independent Rin implementation below. */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && defined(_GLIBCXX_CSTDLIB)
#define RINCXX_CSTDLIB_HOST_NAMESPACE 1
#endif

#if !defined(RINCXX_CSTDLIB_HOST_NAMESPACE)

/* libstdc++'s C compatibility <stdlib.h> imports every std:: name after it
 * includes <cstdlib>.  When Rin owns namespace std, preclaim that forwarding
 * header so a later host <stdlib.h> re-exports the same Rin declarations
 * instead of injecting a second standard-library implementation. */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && \
    __STDC_HOSTED__ && !defined(_GLIBCXX_CSTDLIB)
#define _GLIBCXX_CSTDLIB 1
/* The libstdc++ C compatibility <stdlib.h> wrapper otherwise runs later and
 * imports our independently-defined std:: names into the global namespace.
 * That conflicts with CRT declarations pulled in by pthread/process headers.
 * Rin cstdlib already supplies the C++ declarations it owns, so preclaim the
 * forwarding wrapper as one coherent hosted include-order boundary. */
#define _GLIBCXX_STDLIB_H 1
#endif

/* Hosted C++ consumers can reach this header after a CRT <errno.h> (for
 * example through <istream>'s <iostream> dependency).  Re-declaring Rin's
 * target errno numbers in that translation unit is an ABI and -Werror
 * violation; use the already selected host errno owner there. */
#if !defined(RIN_FREESTANDING) && defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <errno.h>
#else
#include "../libc/errno.h"
#endif
#if defined(RIN_FREESTANDING)
#include "../libc/sys/syscall.h"
#include "../libc/internal/rin_system_owner.h"
#endif
#include "../libunicode/rin_unicode.h"
#if __cplusplus >= 201703L
#include "charconv.h"
#else
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RINCXX_CSTDLIB_PARSE_BINARY80 1
#define RIN_FP_PARSE_BINARY80 1
#endif
#include "../libc/internal/rin_float_parse.h"
#ifdef RINCXX_CSTDLIB_PARSE_BINARY80
#undef RIN_FP_PARSE_BINARY80
#undef RINCXX_CSTDLIB_PARSE_BINARY80
#endif
#endif

#ifndef RAND_MAX
#define RAND_MAX 0x7FFFFFFF
#endif

#ifdef __cplusplus

extern "C" void rin_exit(int status) __attribute__((noreturn));
extern "C" int rand(void);
extern "C" void srand(unsigned int seed);
extern "C" int atexit(void (*func)(void));
extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);
extern "C" void* calloc(size_t nmemb, size_t size);
extern "C" void* realloc(void* ptr, size_t size);
extern "C" size_t malloc_usable_size(void* ptr);
extern "C" int at_quick_exit(void (*func)(void));
extern "C" void quick_exit(int status) __attribute__((noreturn));
#if !defined(environ)
extern "C" char** environ;
#endif
#if !defined(RIN_FREESTANDING)
extern "C" void exit(int status) __attribute__((noreturn));
extern "C" void _Exit(int status) __attribute__((noreturn));
extern "C" void abort(void) __attribute__((noreturn));
extern "C" int system(const char* command);
#endif
#if defined(__MINGW32__)
extern "C" char*** __p__environ(void);
#endif

/* Exit codes as macros to avoid conflict with stdlib.h */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

namespace std {
    using ::size_t;

    namespace __detail {
        /* Some hosted C libraries expose environ as a macro expanding to an
         * accessor expression.  Do not redeclare that macro as a C object;
         * normalize both forms to the table pointer used by std::getenv. */
        inline char** environment_table() {
#if defined(__MINGW32__)
            return *__p__environ();
#elif defined(environ)
            return environ;
#else
            return ::environ;
#endif
        }
    }

    /* ═══════════════════════════════════════════════════════════════
     * Program control - direct syscall implementation
     * ═══════════════════════════════════════════════════════════════*/

    using ::atexit;
    using ::at_quick_exit;
    using ::quick_exit;

#if !defined(RIN_FREESTANDING)
    /* Preserve the host process-control ABI when this header is first.  A
     * later host <stdlib.h> can safely re-export these same entities. */
    using ::exit;
    using ::_Exit;
    using ::abort;
#else
    [[noreturn]] inline void exit(int status) {
        ::rin_exit(status);
    }

    [[noreturn]] inline void _Exit(int status) {
        _syscall1((uintptr_t)SYS_EXIT, (uintptr_t)(unsigned int)status);
        for(;;) {}
    }

    [[noreturn]] inline void abort() {
        _syscall1((uintptr_t)SYS_EXIT, (uintptr_t)1);
        for(;;) {}
    }

#endif

    /* ═══════════════════════════════════════════════════════════════
     * Memory allocation - link to external malloc implementation
     * ═══════════════════════════════════════════════════════════════*/

    using ::malloc;
    using ::free;
    using ::calloc;
    using ::realloc;
    using ::malloc_usable_size;

    /* ═══════════════════════════════════════════════════════════════
     * String conversion
     * ═══════════════════════════════════════════════════════════════*/

    namespace __detail {
        inline bool integer_space(char value) {
            return value == ' ' || value == '\f' || value == '\n' ||
                   value == '\r' || value == '\t' || value == '\v';
        }

        inline int integer_digit(char value) {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'z') return value - 'a' + 10;
            if (value >= 'A' && value <= 'Z') return value - 'A' + 10;
            return -1;
        }

        template<typename Unsigned>
        struct integer_parse_result {
            Unsigned magnitude;
            const char* end;
            bool negative;
            bool converted;
            bool overflow;
        };

        template<typename Unsigned>
        inline integer_parse_result<Unsigned> parse_integer(
            const char* string, int base, Unsigned positive_limit,
            Unsigned negative_limit) {
            integer_parse_result<Unsigned> result = {
                0, string, false, false, false
            };
            const char* cursor = string;
            Unsigned limit;
            Unsigned cutoff;
            Unsigned cutlim;

            while (integer_space(*cursor)) ++cursor;
            if (*cursor == '-' || *cursor == '+') {
                result.negative = *cursor == '-';
                ++cursor;
            }
            limit = result.negative ? negative_limit : positive_limit;

            if ((base == 0 || base == 16) && cursor[0] == '0' &&
                (cursor[1] == 'x' || cursor[1] == 'X')) {
                int first_hex_digit = integer_digit(cursor[2]);
                if (first_hex_digit >= 0 && first_hex_digit < 16) {
                    base = 16;
                    cursor += 2;
                }
            }
            if (base == 0) base = *cursor == '0' ? 8 : 10;

            cutoff = limit / static_cast<Unsigned>(base);
            cutlim = limit % static_cast<Unsigned>(base);
            for (;;) {
                int digit = integer_digit(*cursor);
                Unsigned unsigned_digit;
                if (digit < 0 || digit >= base) break;
                unsigned_digit = static_cast<Unsigned>(digit);
                result.converted = true;
                if (result.magnitude > cutoff ||
                    (result.magnitude == cutoff && unsigned_digit > cutlim)) {
                    result.overflow = true;
                } else if (!result.overflow) {
                    result.magnitude =
                        result.magnitude * static_cast<Unsigned>(base) +
                        unsigned_digit;
                }
                ++cursor;
            }
            if (result.converted) result.end = cursor;
            return result;
        }

        inline bool invalid_integer_input(const char* string, int base) {
            return !string || (base != 0 && (base < 2 || base > 36));
        }

        template<typename Signed, typename Unsigned>
        inline Signed convert_signed_integer(const char* string,
                                             char** endptr, int base,
                                             Unsigned positive_limit) {
            if (invalid_integer_input(string, base)) {
                if (endptr) *endptr = const_cast<char*>(string);
                errno = EINVAL;
                return 0;
            }
            Unsigned negative_limit = positive_limit + 1;
            integer_parse_result<Unsigned> parsed = parse_integer(
                string, base, positive_limit, negative_limit);
            if (endptr) *endptr = const_cast<char*>(parsed.end);
            if (!parsed.converted) return 0;

            Signed positive_max = static_cast<Signed>(positive_limit);
            if (parsed.overflow) {
                errno = ERANGE;
                return parsed.negative ? static_cast<Signed>(-positive_max - 1)
                                       : positive_max;
            }
            if (!parsed.negative) return static_cast<Signed>(parsed.magnitude);
            if (parsed.magnitude == negative_limit) {
                return static_cast<Signed>(-positive_max - 1);
            }
            return static_cast<Signed>(-static_cast<Signed>(parsed.magnitude));
        }

        template<typename Unsigned>
        inline Unsigned convert_unsigned_integer(const char* string,
                                                 char** endptr, int base,
                                                 Unsigned maximum) {
            if (invalid_integer_input(string, base)) {
                if (endptr) *endptr = const_cast<char*>(string);
                errno = EINVAL;
                return 0;
            }
            integer_parse_result<Unsigned> parsed =
                parse_integer(string, base, maximum, maximum);
            if (endptr) *endptr = const_cast<char*>(parsed.end);
            if (!parsed.converted) return 0;
            if (parsed.overflow) {
                errno = ERANGE;
                return maximum;
            }
            return parsed.negative ? static_cast<Unsigned>(0) - parsed.magnitude
                                   : parsed.magnitude;
        }

        /* C++11/14 cannot expose the C++17 charconv parser.  The libc parser
         * has the same classic-locale conversion contract and is independent
         * of C++ language-mode declarations. */
#if __cplusplus >= 201703L
        template<typename Floating>
        inline Floating convert_floating(const char* string, char** endptr) {
            const char* original = string;
            const char* cursor;
            const char* last;
            bool negative = false;
            bool hexadecimal = false;
            Floating converted = static_cast<Floating>(0);
            int range_direction = 0;

            if (!string) {
                if (endptr) *endptr = nullptr;
                errno = EINVAL;
                return converted;
            }
            cursor = string;
            while (integer_space(*cursor)) ++cursor;
            if (*cursor == '-' || *cursor == '+') {
                negative = *cursor == '-';
                ++cursor;
            }
            last = cursor;
            while (*last != '\0') ++last;

            if (cursor[0] == '0' &&
                (cursor[1] == 'x' || cursor[1] == 'X')) {
                int first = integer_digit(cursor[2]);
                int after_point = cursor[2] == '.'
                    ? integer_digit(cursor[3]) : -1;
                hexadecimal = (first >= 0 && first < 16) ||
                              (after_point >= 0 && after_point < 16);
            }

            from_chars_result parsed = hexadecimal
                ? detail::fp_from_chars_hex(cursor + 2, last, converted,
                                            &range_direction)
                : detail::fp_from_chars_decimal(cursor, last, converted,
                                                chars_format::general,
                                                &range_direction);
            if (parsed.ec == errc::invalid_argument) {
                if (endptr) *endptr = const_cast<char*>(original);
                return static_cast<Floating>(0);
            }
            if (endptr) *endptr = const_cast<char*>(parsed.ptr);
            if (parsed.ec == errc::result_out_of_range) {
                errno = ERANGE;
                return range_direction > 0
                    ? detail::fp_infinity_value<Floating>(negative)
                    : detail::fp_zero_value<Floating>(negative);
            }
            if (parsed.ec != errc{}) {
                if (endptr) *endptr = const_cast<char*>(original);
                errno = EINVAL;
                return static_cast<Floating>(0);
            }
            if (negative) converted = -converted;
            if (detail::fp_value_is_subnormal(converted)) errno = ERANGE;
            return converted;
        }
#else
        inline float convert_floating_float(const char* string, char** endptr) {
            rin_float_parse_result parsed = rin_float_parse_binary32(string);
            if (endptr) *endptr = const_cast<char*>(parsed.end);
            if (parsed.invalid_input) {
                if (endptr) *endptr = const_cast<char*>(string);
                errno = EINVAL;
                return 0.0f;
            }
            if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
            return rin_float_parse_binary32_value(parsed.bits);
        }

        inline double convert_floating_double(const char* string, char** endptr) {
            rin_float_parse_result parsed = rin_float_parse_binary64(string);
            if (endptr) *endptr = const_cast<char*>(parsed.end);
            if (parsed.invalid_input) {
                if (endptr) *endptr = const_cast<char*>(string);
                errno = EINVAL;
                return 0.0;
            }
            if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
            return rin_float_parse_binary64_value(parsed.bits);
        }

        inline long double convert_floating_long_double(const char* string,
                                                        char** endptr) {
#if defined(RIN_FP_HAS_BINARY80)
            rin_long_double_parse_result parsed = rin_float_parse_binary80(string);
            if (endptr) *endptr = const_cast<char*>(parsed.end);
            if (parsed.invalid_input) {
                if (endptr) *endptr = const_cast<char*>(string);
                errno = EINVAL;
                return 0.0L;
            }
            if (parsed.range_direction != 0 || parsed.subnormal) errno = ERANGE;
            return parsed.value;
#else
            return static_cast<long double>(
                convert_floating_double(string, endptr));
#endif
        }
#endif
    }

    inline int atoi(const char* s) {
        return __detail::convert_signed_integer<int, unsigned int>(
            s, nullptr, 10, ~0u >> 1u);
    }

    inline long atol(const char* s) {
        return __detail::convert_signed_integer<long, unsigned long>(
            s, nullptr, 10, ~0ul >> 1u);
    }

    inline long long atoll(const char* s) {
        return __detail::convert_signed_integer<long long,
                                                unsigned long long>(
            s, nullptr, 10, ~0ull >> 1u);
    }

    inline double atof(const char* s) {
#if __cplusplus >= 201703L
        return __detail::convert_floating<double>(s, nullptr);
#else
        return __detail::convert_floating_double(s, nullptr);
#endif
    }

    inline long strtol(const char* s, char** endptr, int base) {
        return __detail::convert_signed_integer<long, unsigned long>(
            s, endptr, base, ~0ul >> 1u);
    }

    inline long long strtoll(const char* s, char** endptr, int base) {
        return __detail::convert_signed_integer<long long,
                                                unsigned long long>(
            s, endptr, base, ~0ull >> 1u);
    }

    inline unsigned long strtoul(const char* s, char** endptr, int base) {
        return __detail::convert_unsigned_integer<unsigned long>(
            s, endptr, base, ~0ul);
    }

    inline unsigned long long strtoull(const char* s, char** endptr, int base) {
        return __detail::convert_unsigned_integer<unsigned long long>(
            s, endptr, base, ~0ull);
    }

    inline double strtod(const char* s, char** endptr) {
#if __cplusplus >= 201703L
        return __detail::convert_floating<double>(s, endptr);
#else
        return __detail::convert_floating_double(s, endptr);
#endif
    }

    inline float strtof(const char* s, char** endptr) {
#if __cplusplus >= 201703L
        return __detail::convert_floating<float>(s, endptr);
#else
        return __detail::convert_floating_float(s, endptr);
#endif
    }

    inline long double strtold(const char* s, char** endptr) {
#if __cplusplus >= 201703L
        return __detail::convert_floating<long double>(s, endptr);
#else
        return __detail::convert_floating_long_double(s, endptr);
#endif
    }

    /* ═══════════════════════════════════════════════════════════════
     * Deterministic C random numbers (not a cryptographic generator)
     * ═══════════════════════════════════════════════════════════════*/

    using ::rand;
    using ::srand;

    /* ═══════════════════════════════════════════════════════════════
     * Integer arithmetic
     * Note: abs() overloads are defined in cmath.h to avoid conflicts
     * ═══════════════════════════════════════════════════════════════*/

#ifndef RINCXX_ABS_DEFINED
#define RINCXX_ABS_DEFINED
    inline int abs(int n) {
        if (n >= 0) return n;
        return static_cast<int>(0u - static_cast<unsigned int>(n));
    }
    inline long abs(long n) {
        if (n >= 0) return n;
        return static_cast<long>(0UL - static_cast<unsigned long>(n));
    }
    inline long long abs(long long n) {
        if (n >= 0) return n;
        return static_cast<long long>(0ULL - static_cast<unsigned long long>(n));
    }
#endif

    inline long labs(long n) {
        if (n >= 0) return n;
        return static_cast<long>(0UL - static_cast<unsigned long>(n));
    }
    inline long long llabs(long long n) {
        if (n >= 0) return n;
        return static_cast<long long>(0ULL - static_cast<unsigned long long>(n));
    }

    struct div_t { int quot; int rem; };
    struct ldiv_t { long quot; long rem; };
    struct lldiv_t { long long quot; long long rem; };

    inline div_t div(int numer, int denom) {
        return { numer / denom, numer % denom };
    }

    inline ldiv_t ldiv(long numer, long denom) {
        return { numer / denom, numer % denom };
    }

    inline lldiv_t lldiv(long long numer, long long denom) {
        return { numer / denom, numer % denom };
    }

    /* ═══════════════════════════════════════════════════════════════
     * Search and sort
     * ═══════════════════════════════════════════════════════════════*/

    using cmp_t = int (*)(const void*, const void*);

    namespace __detail {
        inline void qsort_swap(unsigned char* left, unsigned char* right,
                               size_t size) {
            if (left == right) return;
            for (size_t index = 0; index < size; ++index) {
                unsigned char value = left[index];
                left[index] = right[index];
                right[index] = value;
            }
        }

        inline void qsort_sift_down(unsigned char* array, size_t root,
                                    size_t count, size_t size,
                                    cmp_t compare) {
            while (root < count / 2u) {
                size_t child = root * 2u + 1u;
                size_t selected = root;
                if (compare(array + selected * size,
                            array + child * size) < 0) {
                    selected = child;
                }
                if (child + 1u < count &&
                    compare(array + selected * size,
                            array + (child + 1u) * size) < 0) {
                    selected = child + 1u;
                }
                if (selected == root) return;
                qsort_swap(array + root * size, array + selected * size,
                           size);
                root = selected;
            }
        }
    }

    inline void qsort(void* base, size_t nmemb, size_t size, cmp_t compar) {
        unsigned char* arr = (unsigned char*)base;
        if (nmemb < 2u) return;
        if (!arr || !compar || size == 0u || nmemb > SIZE_MAX / size) return;

        for (size_t start = nmemb / 2u; start > 0u; --start) {
            __detail::qsort_sift_down(arr, start - 1u, nmemb, size, compar);
        }
        for (size_t end = nmemb - 1u; end > 0u; --end) {
            __detail::qsort_swap(arr, arr + end * size, size);
            __detail::qsort_sift_down(arr, 0u, end, size, compar);
        }
    }

    inline void* bsearch(const void* key, const void* base, size_t nmemb,
                         size_t size, cmp_t compar) {
        const unsigned char* arr = (const unsigned char*)base;
        if (nmemb == 0u) return nullptr;
        if (!key || !arr || !compar || size == 0u ||
            nmemb > SIZE_MAX / size) {
            return nullptr;
        }
        size_t low = 0, high = nmemb;
        while (low < high) {
            size_t mid = low + (high - low) / 2;
            const void* elem = arr + mid * size;
            int cmp = compar(key, elem);
            if (cmp == 0) return (void*)elem;
            if (cmp < 0) high = mid;
            else low = mid + 1;
        }
        return nullptr;
    }

    /* ═══════════════════════════════════════════════════════════════
     * Environment
     * ═══════════════════════════════════════════════════════════════*/

    namespace __detail {
        constexpr size_t environment_count_max = 128u;
        constexpr size_t environment_string_max = 4096u;
        constexpr size_t environment_bytes_max = 256u * 1024u;

        inline size_t environment_bounded_length(const char* string) {
            if (!string) return SIZE_MAX;
            for (size_t index = 0u; index < environment_string_max;
                 ++index) {
                if (string[index] == '\0') return index;
            }
            return SIZE_MAX;
        }
    }

    inline char* getenv(const char* name) {
        size_t name_length = __detail::environment_bounded_length(name);
        size_t total_bytes = 0u;
        char* result = nullptr;
        char** table = __detail::environment_table();

        if (name_length == 0u || name_length == SIZE_MAX || !table) {
            return nullptr;
        }
        for (size_t index = 0u; index < name_length; ++index) {
            if (name[index] == '=') return nullptr;
        }

        for (size_t entry_index = 0u;
             entry_index < __detail::environment_count_max; ++entry_index) {
            char* entry = table[entry_index];
            size_t entry_length;
            size_t separator = 0u;
            if (!entry) return result;

            entry_length = __detail::environment_bounded_length(entry);
            if (entry_length == 0u || entry_length == SIZE_MAX ||
                total_bytes > __detail::environment_bytes_max -
                                  (entry_length + 1u)) {
                return nullptr;
            }
            total_bytes += entry_length + 1u;
            while (separator < entry_length && entry[separator] != '=') {
                ++separator;
            }
            if (separator == 0u || separator == entry_length) return nullptr;
            if (separator == name_length) {
                size_t index = 0u;
                while (index < name_length && entry[index] == name[index]) {
                    ++index;
                }
                if (index == name_length) {
                    if (result) return nullptr;
                    result = entry + separator + 1u;
                }
            }
        }
        return table[__detail::environment_count_max] == nullptr
                   ? result
                   : nullptr;
    }

    inline int system(const char* command) {
#if !defined(RIN_FREESTANDING)
        /* Hosted builds already have a process command processor.  Delegate
         * both the NULL availability query and command status to that C ABI
         * instead of claiming a shell is unavailable unconditionally. */
        return ::system(command);
#else
        return _rin_system_owner(command);
#endif
    }

    /* ═══════════════════════════════════════════════════════════════
     * Multibyte/wide character conversion (ASCII only)
     * ═══════════════════════════════════════════════════════════════*/

    inline int mblen(const char* s, size_t n) {
        rin_unicode_mbstate_t st = { 0, 0 };
        size_t rc;
        if (!s) return 0;
        rc = rin_unicode_mbrlen(s, n, &st);
        if (rc == (size_t)-1 || rc == (size_t)-2) return -1;
        return (int)rc;
    }

    inline int mbtowc(wchar_t* pwc, const char* s, size_t n) {
        return rin_unicode_mbtowc32((uint32_t*)pwc, s, n);
    }

    inline int wctomb(char* s, wchar_t wc) {
        if (!s) return 0;
        return rin_unicode_wctomb32(s, (uint32_t)wc);
    }

    inline size_t mbstowcs(wchar_t* dest, const char* src, size_t n) {
        return rin_unicode_mbstowcs32((uint32_t*)dest, src, n);
    }

    inline size_t wcstombs(char* dest, const wchar_t* src, size_t n) {
        return rin_unicode_wcstombs32(dest, (const uint32_t*)src, n);
    }

} /* namespace std */

/* <cstdlib> owns the std:: surface.  Re-exporting implementation functions
 * into the global namespace makes a later hosted <stdlib.h> declaration
 * ill-formed, so global C names remain the responsibility of the C header. */

#endif /* __cplusplus */

#endif /* !RINCXX_CSTDLIB_HOST_NAMESPACE */
#endif /* RINCXX_CSTDLIB_H */
