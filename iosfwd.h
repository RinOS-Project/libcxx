/*
 * RinOS C++ <iosfwd> header
 * Forward declarations for I/O streams
 * (Simplified to match RinOS libcxx implementations)
 */

#ifndef RINCXX_IOSFWD_H
#define RINCXX_IOSFWD_H

#include "../libc/stddef.h"
#include "../libc/stdint.h"
#include "pointer_order.h"

namespace std {

#if __cplusplus >= 201402L
#define RIN_IOSFWD_CONSTEXPR14 constexpr
#else
#define RIN_IOSFWD_CONSTEXPR14 inline
#endif

/* C++20 makes the bulk char_traits operations usable during constant
 * evaluation.  Keep the older language modes source-compatible without
 * advertising constexpr support there. */
#if __cplusplus >= 202002L
#define RIN_IOSFWD_CONSTEXPR20 constexpr
#ifndef __cpp_lib_constexpr_char_traits
#define __cpp_lib_constexpr_char_traits 201811L
#endif
#else
#define RIN_IOSFWD_CONSTEXPR20
#endif

/* ═══════════════════════════════════════════════════════════════
 * char_traits - primary template and specializations
 * Must be defined before any use in type aliases
 * ═══════════════════════════════════════════════════════════════*/

// Primary template
template<class CharT> struct char_traits {
    using char_type = CharT;
    using int_type = int;
    /* File offsets must remain 64-bit even on LLP64 hosts where `long` is
     * only 32 bits.  The stream ABI exposes this type through every standard
     * char_traits specialization, so filebuf can preserve large-file
     * positions without narrowing before reaching the backend. */
    using off_type = long long;
    using pos_type = long long;
    using state_type = int;

    static RIN_IOSFWD_CONSTEXPR14 void assign(char_type& c1, const char_type& c2) noexcept {
        c1 = c2;
    }

    static constexpr bool eq(char_type c1, char_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr bool lt(char_type c1, char_type c2) noexcept {
        return c1 < c2;
    }

    static RIN_IOSFWD_CONSTEXPR14 int compare(const char_type* s1, const char_type* s2, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (lt(s1[i], s2[i])) return -1;
            if (lt(s2[i], s1[i])) return 1;
        }
        return 0;
    }

    static RIN_IOSFWD_CONSTEXPR14 size_t length(const char_type* s) {
        size_t len = 0;
        while (s[len]) ++len;
        return len;
    }

    static RIN_IOSFWD_CONSTEXPR14 const char_type* find(const char_type* s, size_t n, const char_type& a) {
        for (size_t i = 0; i < n; ++i) {
            if (eq(s[i], a)) return s + i;
        }
        return nullptr;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* move(
        char_type* dest, const char_type* src, size_t n) {
#if __cplusplus >= 202002L
        /* Relational comparison of pointers into distinct arrays is not a
         * constant expression.  Detect overlap using equality only, then
         * choose the same direction as memmove. */
        if (__builtin_is_constant_evaluated()) {
            bool destination_after_source = false;
            for (size_t i = 0; i <= n; ++i) {
                if (src + i == dest) {
                    destination_after_source = true;
                    break;
                }
            }
            if (destination_after_source) {
                for (size_t i = n; i > 0; --i) dest[i - 1] = src[i - 1];
            } else {
                for (size_t i = 0; i < n; ++i) dest[i] = src[i];
            }
            return dest;
        }
#endif
#if __cplusplus >= 202002L
        if (!__builtin_is_constant_evaluated()) {
            __builtin_memmove(dest, src, n * sizeof(char_type));
            return dest;
        }
#endif
        /* Relational comparison on pointers from distinct arrays is
         * undefined.  Use the target-width address representation for the
         * runtime direction decision; the C++20 constant-evaluation branch
         * above handles overlap without integer casts. */
        if (!detail::object_pointer_total_less(src, dest)) {
            for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        } else {
            for (size_t i = n; i > 0; --i) dest[i-1] = src[i-1];
        }
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* copy(
        char_type* dest, const char_type* src, size_t n) {
        for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* assign(
        char_type* s, size_t n, char_type a) {
        for (size_t i = 0; i < n; ++i) s[i] = a;
        return s;
    }

    static constexpr int_type not_eof(int_type c) noexcept {
        return c == eof() ? 0 : c;
    }

    static constexpr char_type to_char_type(int_type c) noexcept {
        return static_cast<char_type>(c);
    }

    static constexpr int_type to_int_type(char_type c) noexcept {
        return static_cast<int_type>(c);
    }

    static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr int_type eof() noexcept {
        return -1;
    }
};

// Specialization for char
template<>
struct char_traits<char> {
    using char_type = char;
    using int_type = int;
    using off_type = long long;
    using pos_type = long long;
    using state_type = int;

    static RIN_IOSFWD_CONSTEXPR14 void assign(char_type& c1, const char_type& c2) noexcept {
        c1 = c2;
    }

    static constexpr bool eq(char_type c1, char_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr bool lt(char_type c1, char_type c2) noexcept {
        return static_cast<unsigned char>(c1) < static_cast<unsigned char>(c2);
    }

    static RIN_IOSFWD_CONSTEXPR14 int compare(const char_type* s1, const char_type* s2, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            unsigned char c1 = static_cast<unsigned char>(s1[i]);
            unsigned char c2 = static_cast<unsigned char>(s2[i]);
            if (c1 < c2) return -1;
            if (c1 > c2) return 1;
        }
        return 0;
    }

    static RIN_IOSFWD_CONSTEXPR14 size_t length(const char_type* s) {
        size_t len = 0;
        while (s[len]) ++len;
        return len;
    }

    static RIN_IOSFWD_CONSTEXPR14 const char_type* find(const char_type* s, size_t n, const char_type& a) {
        for (size_t i = 0; i < n; ++i) {
            if (eq(s[i], a)) return s + i;
        }
        return nullptr;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* move(
        char_type* dest, const char_type* src, size_t n) {
#if __cplusplus >= 202002L
        if (__builtin_is_constant_evaluated()) {
            bool destination_after_source = false;
            for (size_t i = 0; i <= n; ++i) {
                if (src + i == dest) {
                    destination_after_source = true;
                    break;
                }
            }
            if (destination_after_source) {
                for (size_t i = n; i > 0; --i) dest[i - 1] = src[i - 1];
            } else {
                for (size_t i = 0; i < n; ++i) dest[i] = src[i];
            }
            return dest;
        }
#endif
#if __cplusplus >= 202002L
        if (!__builtin_is_constant_evaluated()) {
            __builtin_memmove(dest, src, n * sizeof(char_type));
            return dest;
        }
#endif
        if (!detail::object_pointer_total_less(src, dest)) {
            for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        } else {
            for (size_t i = n; i > 0; --i) dest[i-1] = src[i-1];
        }
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* copy(
        char_type* dest, const char_type* src, size_t n) {
        for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* assign(
        char_type* s, size_t n, char_type a) {
        for (size_t i = 0; i < n; ++i) s[i] = a;
        return s;
    }

    static constexpr int_type not_eof(int_type c) noexcept {
        return c == eof() ? 0 : c;
    }

    static constexpr char_type to_char_type(int_type c) noexcept {
        return static_cast<char_type>(c);
    }

    static constexpr int_type to_int_type(char_type c) noexcept {
        return static_cast<int_type>(static_cast<unsigned char>(c));
    }

    static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr int_type eof() noexcept {
        return -1;
    }
};

// Specialization for wchar_t
template<>
struct char_traits<wchar_t> {
    using char_type = wchar_t;
    /* Keep the C++ forwarding header independent from the C wide-character
     * implementation.  Hosted C libraries may define wint_t differently
     * from RinOS, while char_traits only requires an unsigned scalar that
     * can represent the RinOS wchar_t code-unit and EOF. */
    using int_type = uint32_t;
    using off_type = long long;
    using pos_type = long long;
    using state_type = int;

    static RIN_IOSFWD_CONSTEXPR14 void assign(char_type& c1, const char_type& c2) noexcept {
        c1 = c2;
    }

    static constexpr bool eq(char_type c1, char_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr bool lt(char_type c1, char_type c2) noexcept {
        return c1 < c2;
    }

    static RIN_IOSFWD_CONSTEXPR14 int compare(const char_type* s1, const char_type* s2, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (lt(s1[i], s2[i])) return -1;
            if (lt(s2[i], s1[i])) return 1;
        }
        return 0;
    }

    static RIN_IOSFWD_CONSTEXPR14 size_t length(const char_type* s) {
        size_t len = 0;
        while (s[len]) ++len;
        return len;
    }

    static RIN_IOSFWD_CONSTEXPR14 const char_type* find(const char_type* s, size_t n, const char_type& a) {
        for (size_t i = 0; i < n; ++i) {
            if (eq(s[i], a)) return s + i;
        }
        return nullptr;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* move(
        char_type* dest, const char_type* src, size_t n) {
#if __cplusplus >= 202002L
        if (__builtin_is_constant_evaluated()) {
            bool destination_after_source = false;
            for (size_t i = 0; i <= n; ++i) {
                if (src + i == dest) {
                    destination_after_source = true;
                    break;
                }
            }
            if (destination_after_source) {
                for (size_t i = n; i > 0; --i) dest[i - 1] = src[i - 1];
            } else {
                for (size_t i = 0; i < n; ++i) dest[i] = src[i];
            }
            return dest;
        }
#endif
#if __cplusplus >= 202002L
        if (!__builtin_is_constant_evaluated()) {
            __builtin_memmove(dest, src, n * sizeof(char_type));
            return dest;
        }
#endif
        if (!detail::object_pointer_total_less(src, dest)) {
            for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        } else {
            for (size_t i = n; i > 0; --i) dest[i-1] = src[i-1];
        }
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* copy(
        char_type* dest, const char_type* src, size_t n) {
        for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* assign(
        char_type* s, size_t n, char_type a) {
        for (size_t i = 0; i < n; ++i) s[i] = a;
        return s;
    }

    static constexpr int_type not_eof(int_type c) noexcept {
        return c == eof() ? 0 : c;
    }

    static constexpr char_type to_char_type(int_type c) noexcept {
        return static_cast<char_type>(c);
    }

    static constexpr int_type to_int_type(char_type c) noexcept {
        return static_cast<int_type>(c);
    }

    static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr int_type eof() noexcept {
        return static_cast<int_type>(-1);
    }
};

/* C++11 adds char16_t/char32_t and C++20 adds char8_t character streams.
 * Keep their traits explicit instead of falling back to the primary template:
 * the standard specializations use an int-like EOF carrier while comparing
 * Unicode code units as unsigned values.  The operations intentionally share
 * the same bounded, constexpr-capable implementation as the narrow/wide
 * specializations above. */
template<typename CharT>
struct rin_iosfwd_unicode_traits {
    using char_type = CharT;
    using int_type = int;
    using off_type = long long;
    using pos_type = long long;
    using state_type = int;

    static RIN_IOSFWD_CONSTEXPR14 void assign(char_type& c1,
                                               const char_type& c2) noexcept {
        c1 = c2;
    }

    static constexpr bool eq(char_type c1, char_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr bool lt(char_type c1, char_type c2) noexcept {
        return c1 < c2;
    }

    static RIN_IOSFWD_CONSTEXPR14 int compare(const char_type* s1,
                                               const char_type* s2,
                                               size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (lt(s1[i], s2[i])) return -1;
            if (lt(s2[i], s1[i])) return 1;
        }
        return 0;
    }

    static RIN_IOSFWD_CONSTEXPR14 size_t length(const char_type* s) {
        size_t len = 0;
        while (s[len]) ++len;
        return len;
    }

    static RIN_IOSFWD_CONSTEXPR14 const char_type* find(
        const char_type* s, size_t n, const char_type& a) {
        for (size_t i = 0; i < n; ++i) {
            if (eq(s[i], a)) return s + i;
        }
        return nullptr;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* move(
        char_type* dest, const char_type* src, size_t n) {
#if __cplusplus >= 202002L
        if (__builtin_is_constant_evaluated()) {
            bool destination_after_source = false;
            for (size_t i = 0; i <= n; ++i) {
                if (src + i == dest) {
                    destination_after_source = true;
                    break;
                }
            }
            if (destination_after_source) {
                for (size_t i = n; i > 0; --i) dest[i - 1] = src[i - 1];
            } else {
                for (size_t i = 0; i < n; ++i) dest[i] = src[i];
            }
            return dest;
        }
#endif
#if __cplusplus >= 202002L
        if (!__builtin_is_constant_evaluated()) {
            __builtin_memmove(dest, src, n * sizeof(char_type));
            return dest;
        }
#endif
        if (!detail::object_pointer_total_less(src, dest)) {
            for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        } else {
            for (size_t i = n; i > 0; --i) dest[i - 1] = src[i - 1];
        }
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* copy(
        char_type* dest, const char_type* src, size_t n) {
        for (size_t i = 0; i < n; ++i) dest[i] = src[i];
        return dest;
    }

    static RIN_IOSFWD_CONSTEXPR20 char_type* assign(
        char_type* s, size_t n, char_type a) {
        for (size_t i = 0; i < n; ++i) s[i] = a;
        return s;
    }

    static constexpr int_type not_eof(int_type c) noexcept {
        return c == eof() ? 0 : c;
    }

    static constexpr char_type to_char_type(int_type c) noexcept {
        return static_cast<char_type>(c);
    }

    static constexpr int_type to_int_type(char_type c) noexcept {
        return static_cast<int_type>(c);
    }

    static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept {
        return c1 == c2;
    }

    static constexpr int_type eof() noexcept { return -1; }
};

template<>
struct char_traits<char16_t> : rin_iosfwd_unicode_traits<char16_t> {};

template<>
struct char_traits<char32_t> : rin_iosfwd_unicode_traits<char32_t> {};

#if defined(__cpp_char8_t)
template<>
struct char_traits<char8_t> : rin_iosfwd_unicode_traits<char8_t> {};
#endif

// Basic string forward declaration (standard compatible 3 params)
template<class CharT, class Traits, class Allocator> class basic_string;

// Allocator forward declaration
template<class T> class allocator;

// Stream type aliases (match ios.h)
using streamoff = long long;
using streamsize = long;

// Basic stream forward declarations (no default args - defined in ios.h)
template<class CharT, class Traits> class basic_ios;
template<class CharT, class Traits> class basic_streambuf;
template<class CharT, class Traits> class basic_istream;
template<class CharT, class Traits> class basic_ostream;
template<class CharT, class Traits> class basic_iostream;

// String stream forward declarations (match standard: CharT, Traits, Allocator)
template<class CharT, class Traits, class Allocator> class basic_stringbuf;
template<class CharT, class Traits, class Allocator> class basic_istringstream;
template<class CharT, class Traits, class Allocator> class basic_ostringstream;
template<class CharT, class Traits, class Allocator> class basic_stringstream;

// File stream forward declarations (no default args)
template<class CharT, class Traits> class basic_filebuf;
template<class CharT, class Traits> class basic_ifstream;
template<class CharT, class Traits> class basic_ofstream;
template<class CharT, class Traits> class basic_fstream;

// Standard type aliases (with explicit Traits)
using ios = basic_ios<char, char_traits<char>>;
using streambuf = basic_streambuf<char, char_traits<char>>;
using istream = basic_istream<char, char_traits<char>>;
using ostream = basic_ostream<char, char_traits<char>>;
using iostream = basic_iostream<char, char_traits<char>>;

using stringbuf = basic_stringbuf<char, char_traits<char>, void>;
using istringstream = basic_istringstream<char, char_traits<char>, void>;
using ostringstream = basic_ostringstream<char, char_traits<char>, void>;
using stringstream = basic_stringstream<char, char_traits<char>, void>;

using filebuf = basic_filebuf<char, char_traits<char>>;
using ifstream = basic_ifstream<char, char_traits<char>>;
using ofstream = basic_ofstream<char, char_traits<char>>;
using fstream = basic_fstream<char, char_traits<char>>;

// Wide character aliases (with explicit Traits)
using wios = basic_ios<wchar_t, char_traits<wchar_t>>;
using wstreambuf = basic_streambuf<wchar_t, char_traits<wchar_t>>;
using wistream = basic_istream<wchar_t, char_traits<wchar_t>>;
using wostream = basic_ostream<wchar_t, char_traits<wchar_t>>;
using wiostream = basic_iostream<wchar_t, char_traits<wchar_t>>;

using wstringbuf = basic_stringbuf<wchar_t, char_traits<wchar_t>, void>;
using wistringstream = basic_istringstream<wchar_t, char_traits<wchar_t>, void>;
using wostringstream = basic_ostringstream<wchar_t, char_traits<wchar_t>, void>;
using wstringstream = basic_stringstream<wchar_t, char_traits<wchar_t>, void>;

using wfilebuf = basic_filebuf<wchar_t, char_traits<wchar_t>>;
using wifstream = basic_ifstream<wchar_t, char_traits<wchar_t>>;
using wofstream = basic_ofstream<wchar_t, char_traits<wchar_t>>;
using wfstream = basic_fstream<wchar_t, char_traits<wchar_t>>;

// fpos template
template<class State> class fpos;
using streampos = fpos<char>;
using wstreampos = fpos<wchar_t>;

} // namespace std

#undef RIN_IOSFWD_CONSTEXPR14
#undef RIN_IOSFWD_CONSTEXPR20

#endif /* RINCXX_IOSFWD_H */
