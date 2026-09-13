/*
 * RinOS C++ <format> ✿
 * 文字列フォーマット (C++20)
 */

#ifndef RINCXX_FORMAT_H
#define RINCXX_FORMAT_H

#include "rincxx.h"

#if __cplusplus >= 202002L
#include "string.h"
#include "string_view.h"
#include "type_traits.h"
#include "utility.h"
#include "stdexcept.h"
#include "array.h"
#include "charconv.h"
#include "pointer_order.h"

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * format_error
 * ═══════════════════════════════════════════════════════════════*/

class format_error : public runtime_error {
public:
    explicit format_error(const string& what_arg) : runtime_error(what_arg) {}
    explicit format_error(const char* what_arg) : runtime_error(what_arg) {}
};

template<typename T, typename CharT>
struct formatter;

template<typename CharT>
class basic_format_parse_context;

namespace detail {

inline constexpr size_t format_max_input_size = 4096;
inline constexpr size_t format_max_field_count = 128;
inline constexpr size_t format_max_spec_size = 64;
/* Input/field grammar stays bounded, while the completed output is limited
 * only by the destination string's allocator and max_size(). */
inline constexpr size_t format_max_output_size = static_cast<size_t>(-1);
inline constexpr size_t format_max_conversion_size = 65536;

/* The runtime formatter already rejects a presentation that the selected
 * built-in formatter cannot parse.  Keep the same grammar at the literal
 * boundary so a typo such as `"{0:x}"` for a string is diagnosed during
 * constant evaluation rather than after the call has been instantiated.
 * Default-constructible user formatters are checked through their constexpr
 * parse owner below; non-constexpr or non-constructible specializations keep
 * the explicit string_view/runtime boundary instead of being guessed here. */
enum class format_literal_kind : unsigned char {
    unsupported,
    integer,
    boolean,
    character,
    floating,
    string,
    pointer
};

template<class T>
struct format_literal_decay {
    using raw = typename remove_cvref<T>::type;
    using type = typename decay<raw>::type;
};

template<class T, class CharT>
struct is_basic_string_view_for : false_type {};

template<class Traits>
struct is_basic_string_view_for<basic_string_view<char, Traits>, char>
    : true_type {};

template<class Traits>
struct is_basic_string_view_for<basic_string_view<wchar_t, Traits>, wchar_t>
    : true_type {};

template<class T, class CharT>
struct is_basic_string_for : false_type {};

template<class Traits, class Allocator>
struct is_basic_string_for<basic_string<char, Traits, Allocator>, char>
    : true_type {};

template<class Traits, class Allocator>
struct is_basic_string_for<basic_string<wchar_t, Traits, Allocator>, wchar_t>
    : true_type {};

template<class T, class CharT>
struct format_literal_kind_of {
    using value_type = typename format_literal_decay<T>::type;
    static constexpr format_literal_kind value =
        is_same<value_type, bool>::value
            ? format_literal_kind::boolean
            : (is_same<value_type, decltype(nullptr)>::value
                   ? format_literal_kind::pointer
                   : (is_same<value_type, CharT>::value
                   ? format_literal_kind::character
                   : (is_floating_point<value_type>::value
                          ? format_literal_kind::floating
                          : (is_integral<value_type>::value
                                 ? format_literal_kind::integer
                                 : ((is_same<CharT, char>::value &&
                                     (is_basic_string_for<value_type, char>::value ||
                                      is_basic_string_view_for<value_type, char>::value)) ||
                                    (is_same<CharT, wchar_t>::value &&
                                     (is_basic_string_for<value_type, wchar_t>::value ||
                                      is_basic_string_view_for<value_type, wchar_t>::value))
                                        ? format_literal_kind::string
                                        : (is_pointer<value_type>::value
                                               ? (is_function<typename remove_pointer<value_type>::type>::value
                                                      ? format_literal_kind::unsupported
                                                      : (is_same<typename remove_cv<
                                                               typename remove_pointer<value_type>::type>::type,
                                                           CharT>::value
                                                                 ? format_literal_kind::string
                                                                 : format_literal_kind::pointer))
                                               : format_literal_kind::unsupported))))));
};

template<class T, class CharT>
struct format_literal_kind_of<T*, CharT> {
    static constexpr format_literal_kind value =
        is_function<T>::value
            ? format_literal_kind::unsupported
            : (is_same<typename remove_cv<T>::type, CharT>::value
                   ? format_literal_kind::string
                   : format_literal_kind::pointer);
};

template<class CharT>
constexpr bool format_spec_is_align(CharT value) {
    return value == static_cast<CharT>('<') ||
           value == static_cast<CharT>('>') ||
           value == static_cast<CharT>('^');
}

/* Numeric formatters additionally accept '=' alignment, which places fill
 * characters after a sign or alternate prefix.  Keep it out of string and
 * pointer grammar: those formatters do not have a prefix boundary. */
template<class CharT>
constexpr bool format_spec_is_numeric_align(CharT value) {
    return format_spec_is_align(value) ||
           value == static_cast<CharT>('=');
}

template<class CharT>
constexpr bool format_spec_is_digit(CharT value) {
    return value >= static_cast<CharT>('0') &&
           value <= static_cast<CharT>('9');
}

template<class CharT>
constexpr bool validate_literal_string_spec(const CharT* spec, size_t length,
                                            bool allow_character = false) {
    size_t cursor = 0u;
    if (cursor + 1u < length && format_spec_is_align(spec[cursor + 1u])) {
        cursor += 2u;
    } else if (cursor < length && format_spec_is_align(spec[cursor])) {
        ++cursor;
    }
    while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    if (cursor < length && spec[cursor] == static_cast<CharT>('.')) {
        ++cursor;
        if (cursor == length || !format_spec_is_digit(spec[cursor])) return false;
        while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    }
    if (cursor < length && spec[cursor] == static_cast<CharT>('s')) ++cursor;
    if (allow_character && cursor < length &&
        spec[cursor] == static_cast<CharT>('c')) {
        ++cursor;
    }
    return cursor == length;
}

template<class CharT>
constexpr bool validate_literal_integer_spec(const CharT* spec, size_t length) {
    size_t cursor = 0u;
    if (cursor + 1u < length &&
        format_spec_is_numeric_align(spec[cursor + 1u])) {
        cursor += 2u;
    } else if (cursor < length && format_spec_is_numeric_align(spec[cursor])) {
        ++cursor;
    }
    if (cursor < length && (spec[cursor] == static_cast<CharT>('+') ||
                            spec[cursor] == static_cast<CharT>('-') ||
                            spec[cursor] == static_cast<CharT>(' '))) {
        ++cursor;
    }
    if (cursor < length && spec[cursor] == static_cast<CharT>('#')) ++cursor;
    if (cursor < length && spec[cursor] == static_cast<CharT>('0')) ++cursor;
    while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    if (cursor < length &&
        (spec[cursor] == static_cast<CharT>('d') ||
         spec[cursor] == static_cast<CharT>('x') ||
         spec[cursor] == static_cast<CharT>('X') ||
         spec[cursor] == static_cast<CharT>('o') ||
         spec[cursor] == static_cast<CharT>('b') ||
         spec[cursor] == static_cast<CharT>('B'))) {
        ++cursor;
    }
    return cursor == length;
}

template<class CharT>
constexpr bool validate_literal_float_spec(const CharT* spec, size_t length) {
    size_t cursor = 0u;
    if (cursor + 1u < length &&
        format_spec_is_numeric_align(spec[cursor + 1u])) {
        cursor += 2u;
    } else if (cursor < length && format_spec_is_numeric_align(spec[cursor])) {
        ++cursor;
    }
    if (cursor < length && (spec[cursor] == static_cast<CharT>('+') ||
                            spec[cursor] == static_cast<CharT>('-') ||
                            spec[cursor] == static_cast<CharT>(' '))) {
        ++cursor;
    }
    if (cursor < length && spec[cursor] == static_cast<CharT>('#')) ++cursor;
    if (cursor < length && spec[cursor] == static_cast<CharT>('0')) ++cursor;
    while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    if (cursor < length && spec[cursor] == static_cast<CharT>('.')) {
        ++cursor;
        if (cursor == length || !format_spec_is_digit(spec[cursor])) return false;
        while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    }
    if (cursor < length &&
        (spec[cursor] == static_cast<CharT>('a') ||
         spec[cursor] == static_cast<CharT>('A') ||
         spec[cursor] == static_cast<CharT>('e') ||
         spec[cursor] == static_cast<CharT>('E') ||
         spec[cursor] == static_cast<CharT>('f') ||
         spec[cursor] == static_cast<CharT>('F') ||
         spec[cursor] == static_cast<CharT>('g') ||
         spec[cursor] == static_cast<CharT>('G'))) {
        ++cursor;
    }
    return cursor == length;
}

template<class CharT>
constexpr bool validate_literal_pointer_spec(const CharT* spec, size_t length) {
    size_t cursor = 0u;
    if (cursor + 1u < length && format_spec_is_align(spec[cursor + 1u])) {
        cursor += 2u;
    } else if (cursor < length && format_spec_is_align(spec[cursor])) {
        ++cursor;
    }
    if (cursor < length && spec[cursor] == static_cast<CharT>('0')) ++cursor;
    while (cursor < length && format_spec_is_digit(spec[cursor])) ++cursor;
    if (cursor < length && spec[cursor] == static_cast<CharT>('p')) ++cursor;
    return cursor == length;
}

template<class CharT, class T>
constexpr bool validate_literal_spec_for_kind(const CharT* spec, size_t length) {
    constexpr format_literal_kind kind = format_literal_kind_of<T, CharT>::value;
    if (kind == format_literal_kind::integer) {
        return validate_literal_integer_spec(spec, length);
    }
    if (kind == format_literal_kind::boolean) {
        return validate_literal_string_spec(spec, length) ||
               validate_literal_integer_spec(spec, length);
    }
    if (kind == format_literal_kind::character) {
        if (length != 0u && spec[length - 1u] == static_cast<CharT>('c')) {
            return validate_literal_string_spec(spec, length - 1u);
        }
        if (length != 0u &&
            (spec[length - 1u] == static_cast<CharT>('b') ||
             spec[length - 1u] == static_cast<CharT>('B') ||
             spec[length - 1u] == static_cast<CharT>('d') ||
             spec[length - 1u] == static_cast<CharT>('o') ||
             spec[length - 1u] == static_cast<CharT>('x') ||
             spec[length - 1u] == static_cast<CharT>('X'))) {
            return validate_literal_integer_spec(spec, length);
        }
        return validate_literal_string_spec(spec, length);
    }
    if (kind == format_literal_kind::floating) {
        return validate_literal_float_spec(spec, length);
    }
    if (kind == format_literal_kind::string) {
        return validate_literal_string_spec(spec, length);
    }
    if (kind == format_literal_kind::pointer) {
        return validate_literal_pointer_spec(spec, length);
    }
    /* A user formatter owns its parse grammar.  When the specialization is
     * default-constructible, invoke its constexpr parse owner here so a
     * direct literal gets the same full-consumption check as runtime format.
     * A non-constructible primary template is not a format-capable type, so
     * direct literals reject it instead of deferring an impossible dispatch. */
    if constexpr (!is_default_constructible<formatter<
                                    typename format_literal_decay<T>::type,
                                    CharT>>::value) {
        return false;
    } else {
        formatter<typename format_literal_decay<T>::type, CharT> selected;
        basic_format_parse_context<CharT> context(
            basic_string_view<CharT>(spec, length));
        return selected.parse(context) == context.end();
    }
}

template<class CharT, size_t Index>
consteval bool validate_literal_spec_at(size_t, const CharT*, size_t) {
    return false;
}

template<class CharT, size_t Index, class Head, class... Tail>
consteval bool validate_literal_spec_at(size_t argument_index,
                                        const CharT* spec, size_t length) {
    if (argument_index == Index) {
        return validate_literal_spec_for_kind<CharT, Head>(spec, length);
    }
    if constexpr (sizeof...(Tail) != 0u) {
        return validate_literal_spec_at<CharT, Index + 1u, Tail...>(
            argument_index, spec, length);
    }
    return false;
}

template<class CharT, size_t Index>
consteval bool validate_literal_dynamic_at(size_t) {
    return false;
}

template<class CharT, size_t Index, class Head, class... Tail>
consteval bool validate_literal_dynamic_at(size_t argument_index) {
    if (argument_index == Index) {
        constexpr format_literal_kind kind =
            format_literal_kind_of<Head, CharT>::value;
        return kind == format_literal_kind::integer ||
               kind == format_literal_kind::boolean ||
               kind == format_literal_kind::character;
    }
    if constexpr (sizeof...(Tail) != 0u) {
        return validate_literal_dynamic_at<CharT, Index + 1u, Tail...>(
            argument_index);
    }
    return false;
}

template<class CharT, class... Args>
consteval bool validate_literal_specs(const CharT* format, size_t length) {
    size_t cursor = 0u;
    size_t automatic_index = 0u;
    bool used_automatic = false;
    bool used_manual = false;
    while (cursor < length) {
        if (format[cursor] != static_cast<CharT>('{')) {
            if (format[cursor] == static_cast<CharT>('}') &&
                cursor + 1u < length &&
                format[cursor + 1u] == static_cast<CharT>('}')) {
                cursor += 2u;
            } else {
                ++cursor;
            }
            continue;
        }
        if (cursor + 1u < length &&
            format[cursor + 1u] == static_cast<CharT>('{')) {
            cursor += 2u;
            continue;
        }
        ++cursor;
        size_t argument_index = 0u;
        bool has_index = false;
        while (cursor < length && format_spec_is_digit(format[cursor])) {
            argument_index = argument_index * 10u +
                             static_cast<size_t>(format[cursor] -
                                                 static_cast<CharT>('0'));
            has_index = true;
            ++cursor;
        }
        if (has_index) {
            if (used_automatic) return false;
            used_manual = true;
        } else {
            if (used_manual) return false;
            used_automatic = true;
            argument_index = automatic_index++;
        }
        size_t spec_begin_cursor = cursor;
        if (cursor < length && format[cursor] == static_cast<CharT>(':')) {
            spec_begin_cursor = ++cursor;
            while (cursor < length && format[cursor] != static_cast<CharT>('}')) {
                if (format[cursor] == static_cast<CharT>('{')) {
                    ++cursor;
                    while (cursor < length && format_spec_is_digit(format[cursor])) ++cursor;
                    if (cursor < length && format[cursor] == static_cast<CharT>('}')) {
                        ++cursor;
                        continue;
                    }
                    return false;
                }
                ++cursor;
            }
        }
        if (cursor >= length || format[cursor] != static_cast<CharT>('}')) return false;
        const CharT* spec_begin = format + spec_begin_cursor;
        const size_t raw_size = cursor - spec_begin_cursor;
        CharT resolved[format_max_spec_size + 1u];
        size_t resolved_size = 0u;
        for (size_t index = 0u; index < raw_size; ++index) {
            const CharT value = spec_begin[index];
            if (value == static_cast<CharT>('{')) {
                size_t nested_index = 0u;
                bool nested_has_index = false;
                size_t nested_cursor = index + 1u;
                while (nested_cursor < raw_size &&
                       format_spec_is_digit(spec_begin[nested_cursor])) {
                    nested_index = nested_index * 10u +
                                   static_cast<size_t>(spec_begin[nested_cursor] -
                                                       static_cast<CharT>('0'));
                    nested_has_index = true;
                    ++nested_cursor;
                }
                if (nested_has_index) {
                    if (used_automatic) return false;
                    used_manual = true;
                } else {
                    if (used_manual) return false;
                    used_automatic = true;
                    nested_index = automatic_index++;
                }
                if (nested_index >= sizeof...(Args) ||
                    !validate_literal_dynamic_at<CharT, 0u, Args...>(
                        nested_index)) return false;
                while (nested_cursor < raw_size &&
                       spec_begin[nested_cursor] != static_cast<CharT>('}')) {
                    ++nested_cursor;
                }
                if (nested_cursor == raw_size) return false;
                index = nested_cursor;
                if (resolved_size == format_max_spec_size) return false;
                resolved[resolved_size++] = static_cast<CharT>('0');
            } else {
                if (resolved_size == format_max_spec_size) return false;
                resolved[resolved_size++] = value;
            }
        }
        if (argument_index >= sizeof...(Args) ||
            !validate_literal_spec_at<CharT, 0u, Args...>(
                argument_index, resolved, resolved_size)) {
            return false;
        }
        ++cursor;
    }
    return true;
}

/* Invalid runtime format input is observable in exception-enabled hosted
 * builds.  The exception-disabled consumer profile deliberately retains the
 * fail-closed boundary instead of pretending formatting succeeded. */
[[noreturn]] inline void format_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw format_error("invalid format string");
#else
    __builtin_trap();
#endif
}

/* C++20 format(string-literal, args...) performs a bounded compile-time
 * grammar and built-in presentation check before the runtime formatter is
 * entered.  Custom formatter presentation checks remain in the runtime
 * formatter's own parse owner. */
template<typename CharT, typename... Args>
consteval bool validate_format_literal(const CharT* format, size_t length,
                                       size_t argument_count) {
    size_t automatic_index = 0;
    bool used_automatic = false;
    bool used_manual = false;

    for (size_t cursor = 0; cursor < length;) {
        const CharT character = format[cursor];
        if (character == static_cast<CharT>('{')) {
            if (cursor + 1 < length &&
                format[cursor + 1] == static_cast<CharT>('{')) {
                cursor += 2;
                continue;
            }

            ++cursor;
            size_t argument_index = 0;
            bool has_argument_index = false;
            while (cursor < length &&
                   format[cursor] >= static_cast<CharT>('0') &&
                   format[cursor] <= static_cast<CharT>('9')) {
                const size_t digit = static_cast<size_t>(
                    format[cursor] - static_cast<CharT>('0'));
                if (argument_index >
                    (static_cast<size_t>(-1) - digit) / 10u) {
                    return false;
                }
                argument_index = argument_index * 10u + digit;
                has_argument_index = true;
                ++cursor;
            }

            if (has_argument_index) {
                if (used_automatic || argument_index >= argument_count)
                    return false;
                used_manual = true;
            } else {
                if (used_manual || automatic_index >= argument_count)
                    return false;
                used_automatic = true;
                argument_index = automatic_index++;
            }

            if (cursor < length &&
                format[cursor] == static_cast<CharT>(':')) {
                ++cursor;
                while (cursor < length) {
                    if (format[cursor] == static_cast<CharT>('{')) {
                        /* A nested replacement field is the only brace form
                         * permitted inside a format spec.  It has no nested
                         * spec of its own and participates in the same
                         * automatic/manual argument-index sequence as the
                         * enclosing field. */
                        ++cursor;
                        size_t nested_index = 0;
                        bool nested_has_index = false;
                        while (cursor < length &&
                               format[cursor] >= static_cast<CharT>('0') &&
                               format[cursor] <= static_cast<CharT>('9')) {
                            const size_t digit = static_cast<size_t>(
                                format[cursor] - static_cast<CharT>('0'));
                            if (nested_index >
                                (static_cast<size_t>(-1) - digit) / 10u) {
                                return false;
                            }
                            nested_index = nested_index * 10u + digit;
                            nested_has_index = true;
                            ++cursor;
                        }
                        if (nested_has_index) {
                            if (used_automatic || nested_index >= argument_count)
                                return false;
                            used_manual = true;
                        } else {
                            if (used_manual || automatic_index >= argument_count)
                                return false;
                            used_automatic = true;
                            nested_index = automatic_index++;
                        }
                        if (cursor >= length ||
                            format[cursor] != static_cast<CharT>('}')) {
                            return false;
                        }
                        ++cursor;
                        continue;
                    }
                    if (format[cursor] == static_cast<CharT>('}')) {
                        break;
                    }
                    ++cursor;
                }
            }

            if (cursor >= length ||
                format[cursor] != static_cast<CharT>('}')) {
                return false;
            }
            ++cursor;
            continue;
        }

        if (character == static_cast<CharT>('}')) {
            if (cursor + 1 >= length ||
                format[cursor + 1] != static_cast<CharT>('}')) {
                return false;
            }
            cursor += 2;
            continue;
        }
        ++cursor;
    }
    return validate_literal_specs<CharT, Args...>(format, length);
}

} /* namespace detail */

/* Compile-time checked format-string carriers. Runtime strings continue to
 * use string_view/vformat so callers can intentionally defer validation. */
template<typename CharT, typename... Args>
class basic_format_string {
    basic_string_view<CharT> view_;

public:
    template<size_t N>
    consteval basic_format_string(const CharT (&format)[N])
        : view_(format, N - 1u) {
        if (!detail::validate_format_literal<CharT, Args...>(
                format, N - 1u, sizeof...(Args))) {
            /* A non-constant trap makes an invalid literal ill-formed while
             * remaining valid in exception-disabled freestanding builds. */
            __builtin_trap();
        }
    }

    constexpr basic_string_view<CharT> get() const noexcept { return view_; }
    constexpr operator basic_string_view<CharT>() const noexcept { return view_; }
};

template<typename... Args>
using format_string = basic_format_string<char, type_identity_t<Args>...>;

template<typename... Args>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Args>...>;

/* ═══════════════════════════════════════════════════════════════
 * format_parse_context
 * フォーマット文字列のパース用コンテキスト
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class basic_format_parse_context {
public:
    using char_type = CharT;
    using const_iterator = const CharT*;
    using iterator = const_iterator;

private:
    const_iterator begin_;
    const_iterator end_;
    size_t next_arg_id_ = 0;
    bool use_auto_indexing_ = true;

public:
    constexpr explicit basic_format_parse_context(basic_string_view<CharT> fmt) noexcept
        : begin_(fmt.data()), end_(fmt.data() + fmt.size()) {}

    constexpr const_iterator begin() const noexcept { return begin_; }
    constexpr const_iterator end() const noexcept { return end_; }

    constexpr void advance_to(const_iterator it) { begin_ = it; }

    constexpr size_t next_arg_id() {
        if (!use_auto_indexing_) {
            detail::format_fail();
        }
        return next_arg_id_++;
    }

    constexpr void check_arg_id(size_t id) {
        if (use_auto_indexing_ && next_arg_id_ > 0) {
            detail::format_fail();
        }
        use_auto_indexing_ = false;
        (void)id;
    }
};

using format_parse_context = basic_format_parse_context<char>;
using wformat_parse_context = basic_format_parse_context<wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * format_context
 * フォーマット出力用コンテキスト
 * ═══════════════════════════════════════════════════════════════*/

/* 前方宣言 */
template<typename Context>
class basic_format_args;

namespace detail {

/* 出力イテレータラッパー */
template<typename CharT>
class format_output_iterator {
    basic_string<CharT>* output_;
    bool* failed_;
    size_t maximum_size_;

public:
    using iterator_category = output_iterator_tag;
    using value_type = void;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = void;

    explicit format_output_iterator(
        basic_string<CharT>& output, bool* failed = nullptr,
        size_t maximum_size = static_cast<size_t>(-1))
        : output_(&output), failed_(failed), maximum_size_(maximum_size) {}

    format_output_iterator& operator=(CharT c) {
        if (failed_ && *failed_) return *this;
        if (output_->size() >= maximum_size_) {
            if (failed_) *failed_ = true;
            return *this;
        }
        const size_t old_size = output_->size();
        output_->push_back(c);
        if (output_->size() != old_size + 1u && failed_) *failed_ = true;
        return *this;
    }

    format_output_iterator& operator*() { return *this; }
    format_output_iterator& operator++() { return *this; }
    format_output_iterator operator++(int) { return *this; }
};

} /* namespace detail */

template<typename OutputIt, typename CharT>
class basic_format_context {
public:
    using char_type = CharT;
    using iterator = OutputIt;

private:
    OutputIt out_;
    basic_format_args<basic_format_context> args_;

public:
    basic_format_context(OutputIt out, basic_format_args<basic_format_context> args)
        : out_(out), args_(args) {}

    iterator out() { return out_; }
    void advance_to(iterator it) { out_ = it; }

    basic_format_args<basic_format_context> args() const { return args_; }

    /* Standard custom formatters may inspect another argument while
     * rendering the current field (for example, a formatter that consumes a
     * user-provided unit or separator).  Keep the lookup borrowed and
     * bounded by the immutable argument store; an out-of-range id returns an
     * empty format_arg and is rejected by the formatter before publication. */
    auto arg(size_t id) const -> decltype(args_.get(id)) {
        return args_.get(id);
    }
};

using format_context = basic_format_context<detail::format_output_iterator<char>, char>;
using wformat_context = basic_format_context<detail::format_output_iterator<wchar_t>, wchar_t>;

/* ═══════════════════════════════════════════════════════════════
 * formatter - 基本テンプレート
 * ═══════════════════════════════════════════════════════════════*/

template<typename T, typename CharT = char>
struct formatter {
    /* 特殊化が必要 - デフォルトはフォーマット不可 */
    formatter() = delete;
};

/* ═══════════════════════════════════════════════════════════════
 * formatter 特殊化 - 整数型
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename CharT>
struct int_formatter {
    int width_ = 0;
    CharT fill_ = ' ';
    char type_ = 'd';
    char align_ = '>';
    bool explicit_align_ = false;
    bool show_sign_ = false;
    bool space_sign_ = false;
    bool show_base_ = false;
    bool zero_padding_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) -> typename ParseContext::iterator {
        auto it = ctx.begin();
        auto end = ctx.end();

        /* フィルと整列 */
        if (it != end && (it + 1) != end) {
            if (*(it + 1) == '<' || *(it + 1) == '>' || *(it + 1) == '^' ||
                *(it + 1) == '=') {
                fill_ = *it++;
            }
        }

        if (it != end && (*it == '<' || *it == '>' || *it == '^' ||
                          *it == '=')) {
            align_ = static_cast<char>(*it);
            explicit_align_ = true;
            ++it;
        }

        /* 符号 */
        if (it != end && (*it == '+' || *it == '-' || *it == ' ')) {
            show_sign_ = (*it == '+');
            space_sign_ = (*it == ' ');
            ++it;
        }

        /* 代替形式 (#) */
        if (it != end && *it == '#') {
            show_base_ = true;
            ++it;
        }

        if (it != end && *it == '0') {
            zero_padding_ = true;
            ++it;
        }

        /* 幅 */
        while (it != end && *it >= '0' && *it <= '9') {
            const int digit = *it - '0';
            if (width_ > (4096 - digit) / 10) return it;
            width_ = width_ * 10 + (*it - '0');
            ++it;
        }

        /* 型指定子 */
        if (it != end && (*it == 'd' || *it == 'x' || *it == 'X' ||
                         *it == 'o' || *it == 'b' || *it == 'B')) {
            type_ = *it++;
        }

        return it;
    }

    template<typename FormatContext, typename T>
    auto format(T value, FormatContext& ctx) -> typename FormatContext::iterator {
        bool negative = false;
        using UnsignedT = typename make_unsigned<T>::type;
        UnsignedT uval;

        if constexpr (is_signed_v<T>) {
            if (value < 0) {
                negative = true;
                uval = UnsignedT(0) - static_cast<UnsignedT>(value);
            } else {
                uval = static_cast<UnsignedT>(value);
            }
        } else {
            uval = value;
        }

        int base = 10;
        const char* digits = "0123456789abcdef";

        if (type_ == 'x') base = 16;
        else if (type_ == 'X') { base = 16; digits = "0123456789ABCDEF"; }
        else if (type_ == 'o') base = 8;
        else if (type_ == 'b' || type_ == 'B') base = 2;

        const bool zero_value = uval == 0;
        CharT buffer[128];
        int pos = 128;
        if (uval == 0) {
            buffer[--pos] = '0';
        } else {
            while (uval > 0) {
                buffer[--pos] = digits[uval % base];
                uval /= base;
            }
        }

        CharT prefix[3];
        int prefix_size = 0;
        if (negative) prefix[prefix_size++] = '-';
        else if (show_sign_) prefix[prefix_size++] = '+';
        else if (space_sign_) prefix[prefix_size++] = ' ';

        if (show_base_) {
            if (base == 16) {
                prefix[prefix_size++] = '0';
                prefix[prefix_size++] = (type_ == 'X') ? 'X' : 'x';
            } else if (base == 8 && !zero_value) {
                prefix[prefix_size++] = '0';
            } else if (base == 2) {
                prefix[prefix_size++] = '0';
                prefix[prefix_size++] = (type_ == 'B') ? 'B' : 'b';
            }
        }

        const int digits_size = 128 - pos;
        const int len = prefix_size + digits_size;
        int padding = (width_ > len) ? (width_ - len) : 0;
        auto out = ctx.out();

        int left_padding = 0;
        int right_padding = 0;
        if (align_ == '<') right_padding = padding;
        else if (align_ == '^') {
            left_padding = padding / 2;
            right_padding = padding - left_padding;
        } else if (align_ == '=') {
            left_padding = 0;
        } else {
            left_padding = padding;
        }

        if (zero_padding_ && !explicit_align_) {
            left_padding = 0;
            for (int index = 0; index < prefix_size; ++index) {
                *out++ = prefix[index];
            }
            for (int index = 0; index < padding; ++index) *out++ = '0';
        } else if (align_ == '=') {
            for (int index = 0; index < prefix_size; ++index) {
                *out++ = prefix[index];
            }
            for (int index = 0; index < padding; ++index) *out++ = fill_;
        } else {
            for (int index = 0; index < left_padding; ++index) *out++ = fill_;
            for (int index = 0; index < prefix_size; ++index) {
                *out++ = prefix[index];
            }
        }

        for (int index = pos; index < 128; ++index) *out++ = buffer[index];
        for (int index = 0; index < right_padding; ++index) *out++ = fill_;

        return out;
    }
};

} /* namespace detail */

/* 整数型のformatter特殊化 */
template<> struct formatter<int, char> : detail::int_formatter<char> {};
template<> struct formatter<unsigned int, char> : detail::int_formatter<char> {};
template<> struct formatter<long, char> : detail::int_formatter<char> {};
template<> struct formatter<unsigned long, char> : detail::int_formatter<char> {};
template<> struct formatter<long long, char> : detail::int_formatter<char> {};
template<> struct formatter<unsigned long long, char> : detail::int_formatter<char> {};
template<> struct formatter<short, char> : detail::int_formatter<char> {};
template<> struct formatter<unsigned short, char> : detail::int_formatter<char> {};

/* The wide formatting path keeps its original character type all the way
 * through formatter dispatch.  Numeric presentations are still rendered by
 * the allocation-free ASCII conversion core, while the output iterator
 * performs the well-defined widening to wchar_t. */
template<> struct formatter<int, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<unsigned int, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<long, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<unsigned long, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<long long, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<unsigned long long, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<short, wchar_t> : detail::int_formatter<wchar_t> {};
template<> struct formatter<unsigned short, wchar_t> : detail::int_formatter<wchar_t> {};

/* ═══════════════════════════════════════════════════════════════
 * formatter specializations - bounded floating point
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

/*
 * The floating formatter deliberately shares the C++20 character conversion
 * owner rather than keeping a second decimal conversion algorithm in format.
 * Its grammar remains bounded to a 4 KiB format string, while the private
 * conversion owner accepts up to 65,536 code units.  The `#` decimal-point
 * form is implemented for finite float/double and supported long-double
 * presentations; locale remains outside this slice.
 */
template<typename CharT>
struct float_formatter {
    static constexpr size_t unspecified_precision = static_cast<size_t>(-1);
    static constexpr size_t conversion_capacity = format_max_conversion_size + 1u;

    size_t width_ = 0u;
    size_t precision_ = unspecified_precision;
    CharT fill_ = static_cast<CharT>(' ');
    char align_ = '>';
    char type_ = 0;
    bool explicit_align_ = false;
    bool show_sign_ = false;
    bool space_sign_ = false;
    bool alternate_ = false;
    bool zero_padding_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) -> typename ParseContext::iterator {
        auto it = ctx.begin();
        const auto end = ctx.end();

        if (it != end && (it + 1) != end &&
            (*(it + 1) == '<' || *(it + 1) == '>' || *(it + 1) == '^' ||
             *(it + 1) == '=')) {
            fill_ = *it++;
        }
        if (it != end && (*it == '<' || *it == '>' || *it == '^' ||
                          *it == '=')) {
            align_ = *it++;
            explicit_align_ = true;
        }
        if (it != end && (*it == '+' || *it == '-' || *it == ' ')) {
            show_sign_ = *it == '+';
            space_sign_ = *it == ' ';
            ++it;
        }
        if (it != end && *it == '#') {
            alternate_ = true;
            ++it;
        }
        if (it != end && *it == '0') {
            zero_padding_ = true;
            ++it;
        }
        while (it != end && *it >= '0' && *it <= '9') {
            const size_t digit = static_cast<size_t>(*it - '0');
            if (width_ > (format_max_conversion_size - digit) / 10u) return it;
            width_ = width_ * 10u + digit;
            ++it;
        }
        if (it != end && *it == '.') {
            const auto mark = it++;
            if (it == end || *it < '0' || *it > '9') return mark;
            precision_ = 0u;
            while (it != end && *it >= '0' && *it <= '9') {
                const size_t digit = static_cast<size_t>(*it - '0');
                if (precision_ > (format_max_conversion_size - digit) / 10u) {
                    return it;
                }
                precision_ = precision_ * 10u + digit;
                ++it;
            }
        }
        if (it != end && (*it == 'a' || *it == 'A' || *it == 'e' ||
                          *it == 'E' || *it == 'f' || *it == 'F' ||
                          *it == 'g' || *it == 'G')) {
            type_ = *it++;
        }
        return it;
    }

    template<typename T>
    static to_chars_result convert(char* first, char* last, T value,
                                   chars_format format, bool has_precision,
                                   size_t precision) {
        if (has_precision) {
            return std::to_chars(first, last, value, format,
                                 static_cast<int>(precision));
        }
        return std::to_chars(first, last, value, format);
    }

    template<typename FormatContext, typename T>
    auto format(T value, FormatContext& ctx) -> typename FormatContext::iterator {
        chars_format conversion_format = chars_format::general;
        bool has_precision = precision_ != unspecified_precision;
        size_t conversion_precision = precision_;
        bool uppercase = false;

        switch (type_) {
            case 'a': conversion_format = chars_format::hex; break;
            case 'A': conversion_format = chars_format::hex; uppercase = true; break;
            case 'e': conversion_format = chars_format::scientific; break;
            case 'E': conversion_format = chars_format::scientific; uppercase = true; break;
            case 'f': conversion_format = chars_format::fixed; break;
            case 'F': conversion_format = chars_format::fixed; uppercase = true; break;
            case 'g': conversion_format = chars_format::general; break;
            case 'G': conversion_format = chars_format::general; uppercase = true; break;
            default: break;
        }

        /* C++ format's explicit decimal presentations default to six digits;
         * the presentation-free form retains charconv's shortest result. */
        if (!has_precision && (type_ == 'e' || type_ == 'E' ||
                               type_ == 'f' || type_ == 'F' ||
                               type_ == 'g' || type_ == 'G')) {
            has_precision = true;
            conversion_precision = 6u;
        }
        /* A general precision of zero means one significant digit. */
        if (alternate_ && has_precision && conversion_precision == 0u &&
            (type_ == 'g' || type_ == 'G')) {
            conversion_precision = 1u;
        }

        string text;
        text.assign(conversion_capacity, '\0');
        if (text.size() != conversion_capacity) format_fail();
        const to_chars_result converted = convert(
            text.data(), text.data() + conversion_capacity, value,
            conversion_format,
            has_precision, conversion_precision);
        if (converted.ec != errc{}) format_fail();
        size_t length = static_cast<size_t>(converted.ptr - text.data());
        if (length == 0u) format_fail();

        if (uppercase) {
            for (size_t index = 0u; index < length; ++index) {
                if (text[index] >= 'a' && text[index] <= 'z') {
                    text[index] = static_cast<char>(text[index] - ('a' - 'A'));
                }
            }
        }

        if (alternate_) {
            const bool decimal_alternate = type_ == 'a' || type_ == 'A' ||
                                           type_ == 'e' || type_ == 'E' ||
                                           type_ == 'f' || type_ == 'F';
            const bool general_alternate = type_ == 'g' || type_ == 'G';
            const bool presentation_free_alternate = type_ == 0;
            if (!decimal_alternate && !general_alternate &&
                !presentation_free_alternate) format_fail();

            const size_t first = text[0] == '-' ? 1u : 0u;
            const bool finite = first < length && text[first] >= '0' &&
                                text[first] <= '9';
            if (finite) {
                size_t decimal = length;
                size_t insertion = length;
                for (size_t index = first; index < length; ++index) {
                    if (text[index] == '.') decimal = index;
                    if (text[index] == 'e' || text[index] == 'E' ||
                        text[index] == 'p' || text[index] == 'P') {
                        insertion = index;
                        break;
                    }
                }

                size_t trailing_zeroes = 0u;
                if (general_alternate) {
                    bool saw_nonzero = false;
                    size_t significant = 0u;
                    for (size_t index = first; index < insertion; ++index) {
                        const char character = text[index];
                        if (character < '0' || character > '9') continue;
                        if (character != '0' || saw_nonzero) {
                            saw_nonzero = true;
                            ++significant;
                        }
                    }
                    if (!saw_nonzero) significant = 1u;
                    if (significant < conversion_precision) {
                        trailing_zeroes = conversion_precision - significant;
                    }
                }

                const size_t decimal_size = decimal == length ? 1u : 0u;
                if (trailing_zeroes > conversion_capacity - length ||
                    decimal_size > conversion_capacity - length - trailing_zeroes) {
                    format_fail();
                }
                const size_t addition = decimal_size + trailing_zeroes;
                if (addition != 0u) {
                    for (size_t index = length; index > insertion; --index) {
                        text[index + addition - 1u] = text[index - 1u];
                    }
                    size_t written = insertion;
                    if (decimal_size != 0u) text[written++] = '.';
                    for (size_t index = 0u; index < trailing_zeroes; ++index) {
                        text[written++] = '0';
                    }
                    length += addition;
                }
            }
        }

        char sign = 0;
        size_t body_begin = 0u;
        if (text[0] == '-') {
            sign = '-';
            body_begin = 1u;
        } else if (show_sign_) {
            sign = '+';
        } else if (space_sign_) {
            sign = ' ';
        }
        const size_t body_size = length - body_begin;
        const size_t content_size = body_size + (sign != 0 ? 1u : 0u);
        const size_t padding = width_ > content_size ? width_ - content_size : 0u;
        size_t left_padding = 0u;
        size_t right_padding = 0u;
        if (align_ == '<') right_padding = padding;
        else if (align_ == '^') {
            left_padding = padding / 2u;
            right_padding = padding - left_padding;
        } else if (align_ == '=') {
            left_padding = 0u;
        } else {
            left_padding = padding;
        }

        auto out = ctx.out();
        if (zero_padding_ && !explicit_align_) {
            if (sign != 0) *out++ = sign;
            for (size_t index = 0u; index < padding; ++index) *out++ = '0';
        } else if (align_ == '=') {
            if (sign != 0) *out++ = sign;
            for (size_t index = 0u; index < padding; ++index) *out++ = fill_;
        } else {
            for (size_t index = 0u; index < left_padding; ++index) *out++ = fill_;
            if (sign != 0) *out++ = sign;
        }
        for (size_t index = body_begin; index < length; ++index) *out++ = text[index];
        for (size_t index = 0u; index < right_padding; ++index) *out++ = fill_;
        return out;
    }
};

} /* namespace detail */

template<> struct formatter<float, char> : detail::float_formatter<char> {};
template<> struct formatter<double, char> : detail::float_formatter<char> {};
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
template<> struct formatter<long double, char> : detail::float_formatter<char> {};
#endif
template<> struct formatter<float, wchar_t> : detail::float_formatter<wchar_t> {};
template<> struct formatter<double, wchar_t> : detail::float_formatter<wchar_t> {};
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
template<> struct formatter<long double, wchar_t> : detail::float_formatter<wchar_t> {};
#endif

/* ═══════════════════════════════════════════════════════════════
 * formatter 特殊化 - 文字・文字列
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

template<typename CharT>
struct basic_string_formatter {
    size_t width_ = 0;
    size_t precision_ = static_cast<size_t>(-1);
    CharT fill_ = CharT(' ');
    char align_ = '<';

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        auto it = ctx.begin();
        auto end = ctx.end();

        if (it != end && (it + 1) != end &&
            (*(it + 1) == '<' || *(it + 1) == '>' || *(it + 1) == '^')) {
            fill_ = *it++;
        }
        if (it != end && (*it == '<' || *it == '>' || *it == '^')) {
            align_ = *it++;
        }

        while (it != end && *it >= '0' && *it <= '9') {
            const size_t digit = static_cast<size_t>(*it - '0');
            if (width_ > (format_max_conversion_size - digit) / 10u) return it;
            width_ = width_ * 10u + digit;
            ++it;
        }

        if (it != end && *it == '.') {
            auto precision_mark = it++;
            if (it == end || *it < '0' || *it > '9') return precision_mark;
            precision_ = 0;
            while (it != end && *it >= '0' && *it <= '9') {
                const size_t digit = static_cast<size_t>(*it - '0');
                if (precision_ > (format_max_conversion_size - digit) / 10u) return it;
                precision_ = precision_ * 10u + digit;
                ++it;
            }
        }

        if (it != end && *it == CharT('s')) ++it;
        return it;
    }

    template<typename FormatContext>
    auto format_view(const CharT* data, size_t size, FormatContext& ctx) {
        auto out = ctx.out();
        const size_t shown = precision_ < size ? precision_ : size;
        const size_t padding = width_ > shown ? width_ - shown : 0;
        size_t left_padding = 0;
        size_t right_padding = 0;

        if (align_ == '<') right_padding = padding;
        else if (align_ == '^') {
            left_padding = padding / 2u;
            right_padding = padding - left_padding;
        } else left_padding = padding;

        for (size_t i = 0; i < left_padding; ++i) *out++ = fill_;
        for (size_t i = 0; i < shown; ++i) *out++ = data[i];
        for (size_t i = 0; i < right_padding; ++i) *out++ = fill_;

        return out;
    }
};

using string_formatter = basic_string_formatter<char>;

} /* namespace detail */

template<>
struct formatter<const char*, char> : detail::string_formatter {
    template<typename FormatContext>
    auto format(const char* s, FormatContext& ctx) {
        if (!s) detail::format_fail();
        size_t size = 0;
        while (size < detail::format_max_conversion_size && s[size]) ++size;
        if (size == detail::format_max_conversion_size && s[size]) {
            detail::format_fail();
        }
        return format_view(s, size, ctx);
    }
};

template<>
struct formatter<string, char> : formatter<const char*, char> {
    template<typename FormatContext>
    auto format(const string& s, FormatContext& ctx) {
        return this->format_view(s.data(), s.size(), ctx);
    }
};

template<typename Traits>
struct formatter<basic_string_view<char, Traits>, char>
    : formatter<const char*, char> {
    template<typename FormatContext>
    auto format(basic_string_view<char, Traits> s, FormatContext& ctx) {
        if (!s.data() && s.size() != 0) detail::format_fail();
        return this->format_view(s.data(), s.size(), ctx);
    }
};

template<typename Traits, typename Allocator>
struct formatter<basic_string<char, Traits, Allocator>, char>
    : formatter<const char*, char> {
    template<typename FormatContext>
    auto format(const basic_string<char, Traits, Allocator>& s,
                FormatContext& ctx) {
        return this->format_view(s.data(), s.size(), ctx);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * formatter 特殊化 - char
 * ═══════════════════════════════════════════════════════════════*/

template<>
struct formatter<char, char> {
    detail::string_formatter string_formatter_;
    detail::int_formatter<char> integer_formatter_;
    bool numeric_presentation_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        const auto begin = ctx.begin();
        const auto end = ctx.end();
        if (begin != end) {
            const char type = *(end - 1);
            if (type == 'b' || type == 'B' || type == 'd' || type == 'o' ||
                type == 'x' || type == 'X') {
                numeric_presentation_ = true;
                return integer_formatter_.parse(ctx);
            }
            if (type == 'c') {
                format_parse_context layout_context(
                    string_view(begin, static_cast<size_t>(end - begin - 1)));
                if (string_formatter_.parse(layout_context) !=
                    layout_context.end()) {
                    return begin;
                }
                return end;
            }
        }
        return string_formatter_.parse(ctx);
    }

    template<typename FormatContext>
    auto format(char c, FormatContext& ctx) {
        if (numeric_presentation_) {
            return integer_formatter_.format(
                static_cast<unsigned int>(static_cast<unsigned char>(c)), ctx);
        }
        return string_formatter_.format_view(&c, 1u, ctx);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * formatter 特殊化 - bool
 * ═══════════════════════════════════════════════════════════════*/

template<>
struct formatter<bool, char> {
    detail::string_formatter string_formatter_;
    detail::int_formatter<char> integer_formatter_;
    bool numeric_presentation_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        const auto begin = ctx.begin();
        const auto end = ctx.end();
        if (begin != end) {
            const char type = *(end - 1);
            if (type == 'b' || type == 'B' || type == 'd' || type == 'o' ||
                type == 'x' || type == 'X') {
                numeric_presentation_ = true;
                return integer_formatter_.parse(ctx);
            }
        }
        return string_formatter_.parse(ctx);
    }

    template<typename FormatContext>
    auto format(bool b, FormatContext& ctx) {
        if (numeric_presentation_) {
            return integer_formatter_.format(static_cast<unsigned int>(b), ctx);
        }
        const char* s = b ? "true" : "false";
        return string_formatter_.format_view(s, b ? 4u : 5u, ctx);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * formatter 特殊化 - ポインタ
 * ═══════════════════════════════════════════════════════════════*/

template<>
struct formatter<void*, char> {
    int width_ = 0;
    char fill_ = ' ';
    char align_ = '>';
    bool explicit_align_ = false;
    bool zero_padding_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        auto it = ctx.begin();
        const auto end = ctx.end();

        if (it != end && (it + 1) != end &&
            (*(it + 1) == '<' || *(it + 1) == '>' || *(it + 1) == '^')) {
            fill_ = *it++;
        }
        if (it != end && (*it == '<' || *it == '>' || *it == '^')) {
            align_ = *it++;
            explicit_align_ = true;
        }
        if (it != end && *it == '0') {
            zero_padding_ = true;
            ++it;
        }
        while (it != end && *it >= '0' && *it <= '9') {
            const int digit = *it - '0';
            if (width_ > (4096 - digit) / 10) return it;
            width_ = width_ * 10 + digit;
            ++it;
        }
        if (it != end && *it == 'p') ++it;
        return it;
    }

    template<typename FormatContext>
    auto format(void* p, FormatContext& ctx) {
        char buffer[2 * sizeof(uintptr_t)];
        int pos = static_cast<int>(sizeof(buffer));
        uintptr_t val = detail::object_pointer_hash(p);
        const char* digits = "0123456789abcdef";

        if (val == 0) {
            buffer[--pos] = '0';
        } else {
            while (val > 0) {
                buffer[--pos] = digits[val & 0xF];
                val >>= 4;
            }
        }

        const int digit_count = static_cast<int>(sizeof(buffer)) - pos;
        const int content_size = 2 + digit_count;
        const int padding = width_ > content_size ? width_ - content_size : 0;
        int left_padding = 0;
        int right_padding = 0;
        if (align_ == '<') right_padding = padding;
        else if (align_ == '^') {
            left_padding = padding / 2;
            right_padding = padding - left_padding;
        } else {
            left_padding = padding;
        }

        auto out = ctx.out();
        if (zero_padding_ && !explicit_align_) {
            *out++ = '0';
            *out++ = 'x';
            for (int index = 0; index < padding; ++index) *out++ = '0';
        } else {
            for (int index = 0; index < left_padding; ++index) *out++ = fill_;
            *out++ = '0';
            *out++ = 'x';
        }
        for (int index = pos; index < static_cast<int>(sizeof(buffer)); ++index) {
            *out++ = buffer[index];
        }
        for (int index = 0; index < right_padding; ++index) *out++ = fill_;
        return out;
    }
};

template<>
struct formatter<const void*, char> : formatter<void*, char> {
    template<typename FormatContext>
    auto format(const void* p, FormatContext& ctx) {
        return formatter<void*, char>::format(const_cast<void*>(p), ctx);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * format_arg - 型消去された引数
 * ═══════════════════════════════════════════════════════════════*/

namespace detail {

enum class format_arg_type : unsigned char {
    none,
    int_type,
    unsigned_type,
    long_long_type,
    unsigned_long_long_type,
    bool_type,
    char_type,
    float_type,
    double_type,
    long_double_type,
    cstring_type,
    string_type,
    pointer_type,
    custom_type
};

struct format_string_value {
    const char* data;
    size_t size;
};

/* A custom format argument borrows the object for the duration of the
 * formatting call.  Its dispatcher owns the formatter specialization, so the
 * runtime argument store never has to know the object's size or layout. */
struct format_custom_value {
    const void* object;
    void (*format)(const void*, string_view, format_context&);
};

template<typename T>
inline void format_custom_dispatch(const void* object, string_view spec,
                                   format_context& context) {
    if (!object) format_fail();
    formatter<T, char> selected;
    format_parse_context parse_context(spec);
    if (selected.parse(parse_context) != parse_context.end()) format_fail();
    (void)selected.format(*static_cast<const T*>(object), context);
}

} /* namespace detail */

template<typename Context>
class basic_format_arg {
    detail::format_arg_type type_ = detail::format_arg_type::none;
    union {
        int int_value;
        unsigned unsigned_value;
        long long long_long_value;
        unsigned long long unsigned_long_long_value;
        bool bool_value;
        char char_value;
        float float_value;
        double double_value;
        long double long_double_value;
        const char* cstring_value;
        detail::format_string_value string_value;
        const void* pointer_value;
        detail::format_custom_value custom_value;
    } value_;

public:
    basic_format_arg() = default;

    explicit operator bool() const noexcept {
        return type_ != detail::format_arg_type::none;
    }

    /* 型ごとのコンストラクタヘルパー */
    static basic_format_arg make(int v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::int_type;
        arg.value_.int_value = v;
        return arg;
    }

    static basic_format_arg make(unsigned v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::unsigned_type;
        arg.value_.unsigned_value = v;
        return arg;
    }

    static basic_format_arg make(long v) {
        return make(static_cast<long long>(v));
    }

    static basic_format_arg make(unsigned long v) {
        return make(static_cast<unsigned long long>(v));
    }

    static basic_format_arg make(long long v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::long_long_type;
        arg.value_.long_long_value = v;
        return arg;
    }

    static basic_format_arg make(unsigned long long v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::unsigned_long_long_type;
        arg.value_.unsigned_long_long_value = v;
        return arg;
    }

    static basic_format_arg make(bool v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::bool_type;
        arg.value_.bool_value = v;
        return arg;
    }

    static basic_format_arg make(char v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::char_type;
        arg.value_.char_value = v;
        return arg;
    }

    static basic_format_arg make(float v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::float_type;
        arg.value_.float_value = v;
        return arg;
    }

    static basic_format_arg make(double v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::double_type;
        arg.value_.double_value = v;
        return arg;
    }

#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
    static basic_format_arg make(long double v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::long_double_type;
        arg.value_.long_double_value = v;
        return arg;
    }
#else
    static basic_format_arg make(long double) = delete;
#endif

    static basic_format_arg make(const char* v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::cstring_type;
        arg.value_.cstring_value = v;
        return arg;
    }

    static basic_format_arg make(char* v) {
        return make(static_cast<const char*>(v));
    }

    static basic_format_arg make(const string& v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::string_type;
        arg.value_.string_value = {v.data(), v.size()};
        return arg;
    }

    static basic_format_arg make(string_view v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::string_type;
        arg.value_.string_value = {v.data(), v.size()};
        return arg;
    }

    template<typename Traits, typename Allocator>
    static basic_format_arg make(
        const basic_string<char, Traits, Allocator>& v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::string_type;
        arg.value_.string_value = {v.data(), v.size()};
        return arg;
    }

    template<typename Traits>
    static basic_format_arg make(basic_string_view<char, Traits> v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::string_type;
        arg.value_.string_value = {v.data(), v.size()};
        return arg;
    }

    static basic_format_arg make(const void* v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::pointer_type;
        arg.value_.pointer_value = v;
        return arg;
    }

    /* `nullptr` is pointer-formattable, but it otherwise converts equally to
     * every pointer/string overload above.  Keep the null literal on the
     * same erased pointer path without relying on ambiguous overload
     * resolution in the argument-store builder. */
    static basic_format_arg make(decltype(nullptr)) {
        return make(static_cast<const void*>(nullptr));
    }

    template<typename T, typename U = remove_cvref_t<T>,
             enable_if_t<!is_arithmetic<U>::value && !is_pointer<U>::value &&
                             !is_same<U, string>::value &&
                             !is_same<U, string_view>::value &&
                             !detail::is_basic_string_for<U, char>::value &&
                             !detail::is_basic_string_view_for<U, char>::value &&
                             !is_volatile<remove_reference_t<T>>::value &&
                             is_default_constructible<formatter<U, char>>::value,
                         int> = 0>
    static basic_format_arg make(T&& v) {
        basic_format_arg arg;
        arg.type_ = detail::format_arg_type::custom_type;
        arg.value_.custom_value = {
            static_cast<const void*>(&v), &detail::format_custom_dispatch<U>};
        return arg;
    }

    bool is_custom() const noexcept {
        return type_ == detail::format_arg_type::custom_type;
    }

    void format_custom(string_view spec, format_context& context) const {
        if (type_ != detail::format_arg_type::custom_type ||
            !value_.custom_value.object || !value_.custom_value.format) {
            detail::format_fail();
        }
        value_.custom_value.format(value_.custom_value.object, spec, context);
    }

    template<typename Visitor>
    auto visit(Visitor&& vis) const {
        switch (type_) {
            case detail::format_arg_type::int_type:
                return vis(value_.int_value);
            case detail::format_arg_type::unsigned_type:
                return vis(value_.unsigned_value);
            case detail::format_arg_type::long_long_type:
                return vis(value_.long_long_value);
            case detail::format_arg_type::unsigned_long_long_type:
                return vis(value_.unsigned_long_long_value);
            case detail::format_arg_type::bool_type:
                return vis(value_.bool_value);
            case detail::format_arg_type::char_type:
                return vis(value_.char_value);
            case detail::format_arg_type::float_type:
                return vis(value_.float_value);
            case detail::format_arg_type::double_type:
                return vis(value_.double_value);
            case detail::format_arg_type::long_double_type:
                return vis(value_.long_double_value);
            case detail::format_arg_type::cstring_type:
                return vis(value_.cstring_value);
            case detail::format_arg_type::string_type:
                return vis(string_view(value_.string_value.data,
                                       value_.string_value.size));
            case detail::format_arg_type::pointer_type:
                return vis(value_.pointer_value);
            default:
                detail::format_fail();
        }
    }
};

using format_arg = basic_format_arg<format_context>;

/*
 * The wide formatting entry points use the same bounded parser and formatter
 * owner as the narrow path.  Arguments are kept as a small, non-owning wide
 * value store until the render transaction converts the ASCII subset to the
 * existing char formatter.  This keeps the wchar ABI independent while
 * retaining the parser's field/spec/output limits and failure-atomic result.
 */
namespace detail {

struct wide_format_string_value {
    const wchar_t* data;
    size_t size;
};

} /* namespace detail */

template<>
struct formatter<void*, wchar_t> {
    int width_ = 0;
    wchar_t fill_ = L' ';
    wchar_t align_ = L'>';
    bool explicit_align_ = false;
    bool zero_padding_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        auto it = ctx.begin();
        const auto end = ctx.end();
        if (it != end && (it + 1) != end &&
            (*(it + 1) == L'<' || *(it + 1) == L'>' || *(it + 1) == L'^')) {
            fill_ = *it++;
        }
        if (it != end && (*it == L'<' || *it == L'>' || *it == L'^')) {
            align_ = *it++;
            explicit_align_ = true;
        }
        if (it != end && *it == L'0') {
            zero_padding_ = true;
            ++it;
        }
        while (it != end && *it >= L'0' && *it <= L'9') {
            const int digit = static_cast<int>(*it - L'0');
            if (width_ > (4096 - digit) / 10) return it;
            width_ = width_ * 10 + digit;
            ++it;
        }
        if (it != end && *it == L'p') ++it;
        return it;
    }

    template<typename FormatContext>
    auto format(void* p, FormatContext& ctx) {
        wchar_t buffer[2 * sizeof(uintptr_t)];
        constexpr int buffer_size = static_cast<int>(2u * sizeof(uintptr_t));
        int pos = buffer_size;
        uintptr_t value = detail::object_pointer_hash(p);
        const wchar_t* digits = L"0123456789abcdef";
        if (value == 0u) {
            buffer[--pos] = L'0';
        } else {
            while (value > 0u) {
                buffer[--pos] = digits[value & 0xFu];
                value >>= 4;
            }
        }
        const int digit_count = buffer_size - pos;
        const int content_size = 2 + digit_count;
        const int padding = width_ > content_size ? width_ - content_size : 0;
        int left_padding = 0;
        int right_padding = 0;
        if (align_ == L'<') right_padding = padding;
        else if (align_ == L'^') {
            left_padding = padding / 2;
            right_padding = padding - left_padding;
        } else {
            left_padding = padding;
        }
        auto out = ctx.out();
        if (zero_padding_ && !explicit_align_) {
            *out++ = L'0';
            *out++ = L'x';
            for (int index = 0; index < padding; ++index) *out++ = L'0';
        } else {
            for (int index = 0; index < left_padding; ++index) *out++ = fill_;
            *out++ = L'0';
            *out++ = L'x';
        }
        for (int index = pos; index < buffer_size; ++index)
            *out++ = buffer[index];
        for (int index = 0; index < right_padding; ++index) *out++ = fill_;
        return out;
    }
};

template<>
struct formatter<const void*, wchar_t> : formatter<void*, wchar_t> {
    template<typename FormatContext>
    auto format(const void* p, FormatContext& ctx) {
        return formatter<void*, wchar_t>::format(const_cast<void*>(p), ctx);
    }
};

template<>
struct formatter<wchar_t, wchar_t> {
    detail::basic_string_formatter<wchar_t> string_formatter_;
    detail::int_formatter<wchar_t> integer_formatter_;
    bool numeric_presentation_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        const auto begin = ctx.begin();
        const auto end = ctx.end();
        if (begin != end) {
            const wchar_t type = *(end - 1);
            if (type == L'b' || type == L'B' || type == L'd' || type == L'o' ||
                type == L'x' || type == L'X') {
                numeric_presentation_ = true;
                return integer_formatter_.parse(ctx);
            }
        }
        return string_formatter_.parse(ctx);
    }

    template<typename FormatContext>
    auto format(wchar_t c, FormatContext& ctx) {
        if (numeric_presentation_) {
            return integer_formatter_.format(
                static_cast<unsigned int>(c), ctx);
        }
        return string_formatter_.format_view(&c, 1u, ctx);
    }
};

template<>
struct formatter<bool, wchar_t> {
    detail::basic_string_formatter<wchar_t> string_formatter_;
    detail::int_formatter<wchar_t> integer_formatter_;
    bool numeric_presentation_ = false;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        const auto begin = ctx.begin();
        const auto end = ctx.end();
        if (begin != end) {
            const wchar_t type = *(end - 1);
            if (type == L'b' || type == L'B' || type == L'd' || type == L'o' ||
                type == L'x' || type == L'X') {
                numeric_presentation_ = true;
                return integer_formatter_.parse(ctx);
            }
        }
        return string_formatter_.parse(ctx);
    }

    template<typename FormatContext>
    auto format(bool value, FormatContext& ctx) {
        if (numeric_presentation_) {
            return integer_formatter_.format(static_cast<unsigned int>(value), ctx);
        }
        const wchar_t* text = value ? L"true" : L"false";
        return string_formatter_.format_view(text, value ? 4u : 5u, ctx);
    }
};

template<>
struct formatter<const wchar_t*, wchar_t> : detail::basic_string_formatter<wchar_t> {
    template<typename FormatContext>
    auto format(const wchar_t* s, FormatContext& ctx) {
        if (!s) detail::format_fail();
        size_t size = 0u;
        while (size < detail::format_max_conversion_size && s[size]) ++size;
        if (size == detail::format_max_conversion_size && s[size]) {
            detail::format_fail();
        }
        return this->format_view(s, size, ctx);
    }
};

template<>
struct formatter<wstring, wchar_t> : formatter<const wchar_t*, wchar_t> {
    template<typename FormatContext>
    auto format(const wstring& s, FormatContext& ctx) {
        return this->format_view(s.data(), s.size(), ctx);
    }
};

template<typename Traits>
struct formatter<basic_string_view<wchar_t, Traits>, wchar_t>
    : formatter<const wchar_t*, wchar_t> {
    template<typename FormatContext>
    auto format(basic_string_view<wchar_t, Traits> s, FormatContext& ctx) {
        if (!s.data() && s.size() != 0u) detail::format_fail();
        return this->format_view(s.data(), s.size(), ctx);
    }
};

template<typename Traits, typename Allocator>
struct formatter<basic_string<wchar_t, Traits, Allocator>, wchar_t>
    : formatter<const wchar_t*, wchar_t> {
    template<typename FormatContext>
    auto format(const basic_string<wchar_t, Traits, Allocator>& s,
                FormatContext& ctx) {
        return this->format_view(s.data(), s.size(), ctx);
    }
};

namespace detail {

/* Wide custom arguments borrow the caller object for the duration of one
 * render transaction.  Keeping the formatter dispatcher beside the erased
 * value preserves the same lifetime and parse-error boundary as the narrow
 * custom path. */
struct wide_format_custom_value {
    const void* object;
    void (*format)(const void*, wstring_view, wformat_context&);
};

enum class wide_format_arg_type : unsigned char {
    none,
    int_type,
    unsigned_type,
    long_long_type,
    unsigned_long_long_type,
    bool_type,
    char_type,
    float_type,
    double_type,
    long_double_type,
    cstring_type,
    string_type,
    pointer_type,
    custom_type
};

template<typename T>
inline void wide_format_custom_dispatch(const void* object,
                                        wstring_view spec,
                                        wformat_context& context) {
    if (!object) format_fail();
    formatter<T, wchar_t> selected;
    wformat_parse_context parse_context(spec);
    if (selected.parse(parse_context) != parse_context.end()) format_fail();
    (void)selected.format(*static_cast<const T*>(object), context);
}

} /* namespace detail */

class wide_format_arg {
    detail::wide_format_arg_type type_ = detail::wide_format_arg_type::none;
    union {
        int int_value;
        unsigned unsigned_value;
        long long long_long_value;
        unsigned long long unsigned_long_long_value;
        bool bool_value;
        wchar_t char_value;
        float float_value;
        double double_value;
        long double long_double_value;
        const wchar_t* cstring_value;
        detail::wide_format_string_value string_value;
        const void* pointer_value;
        detail::wide_format_custom_value custom_value;
    } value_;

public:
    wide_format_arg() = default;

    explicit operator bool() const noexcept {
        return type_ != detail::wide_format_arg_type::none;
    }

    detail::wide_format_arg_type type() const noexcept { return type_; }

    static wide_format_arg make(int value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::int_type;
        arg.value_.int_value = value;
        return arg;
    }
    static wide_format_arg make(unsigned value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::unsigned_type;
        arg.value_.unsigned_value = value;
        return arg;
    }
    static wide_format_arg make(long value) {
        return make(static_cast<long long>(value));
    }
    static wide_format_arg make(unsigned long value) {
        return make(static_cast<unsigned long long>(value));
    }
    static wide_format_arg make(long long value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::long_long_type;
        arg.value_.long_long_value = value;
        return arg;
    }
    static wide_format_arg make(unsigned long long value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::unsigned_long_long_type;
        arg.value_.unsigned_long_long_value = value;
        return arg;
    }
    static wide_format_arg make(bool value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::bool_type;
        arg.value_.bool_value = value;
        return arg;
    }
    static wide_format_arg make(wchar_t value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::char_type;
        arg.value_.char_value = value;
        return arg;
    }
    static wide_format_arg make(float value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::float_type;
        arg.value_.float_value = value;
        return arg;
    }
    static wide_format_arg make(double value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::double_type;
        arg.value_.double_value = value;
        return arg;
    }
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
    static wide_format_arg make(long double value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::long_double_type;
        arg.value_.long_double_value = value;
        return arg;
    }
#endif
    static wide_format_arg make(const wchar_t* value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::cstring_type;
        arg.value_.cstring_value = value;
        return arg;
    }
    static wide_format_arg make(wchar_t* value) {
        return make(static_cast<const wchar_t*>(value));
    }
    static wide_format_arg make(const wstring& value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::string_type;
        arg.value_.string_value = {value.data(), value.size()};
        return arg;
    }
    static wide_format_arg make(wstring_view value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::string_type;
        arg.value_.string_value = {value.data(), value.size()};
        return arg;
    }

    template<typename Traits, typename Allocator>
    static wide_format_arg make(
        const basic_string<wchar_t, Traits, Allocator>& value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::string_type;
        arg.value_.string_value = {value.data(), value.size()};
        return arg;
    }

    template<typename Traits>
    static wide_format_arg make(basic_string_view<wchar_t, Traits> value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::string_type;
        arg.value_.string_value = {value.data(), value.size()};
        return arg;
    }
    static wide_format_arg make(const void* value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::pointer_type;
        arg.value_.pointer_value = value;
        return arg;
    }

    static wide_format_arg make(decltype(nullptr)) {
        return make(static_cast<const void*>(nullptr));
    }

    template<typename T, typename U = remove_cvref_t<T>,
             enable_if_t<!is_arithmetic<U>::value && !is_pointer<U>::value &&
                             !is_same<U, wstring>::value &&
                             !is_same<U, wstring_view>::value &&
                             !detail::is_basic_string_for<U, wchar_t>::value &&
                             !detail::is_basic_string_view_for<U, wchar_t>::value &&
                             !is_volatile<remove_reference_t<T>>::value &&
                             is_default_constructible<formatter<U, wchar_t>>::value,
                         int> = 0>
    static wide_format_arg make(T&& value) {
        wide_format_arg arg;
        arg.type_ = detail::wide_format_arg_type::custom_type;
        arg.value_.custom_value = {
            static_cast<const void*>(&value),
            &detail::wide_format_custom_dispatch<U>};
        return arg;
    }

    bool is_custom() const noexcept {
        return type_ == detail::wide_format_arg_type::custom_type;
    }

    void format_custom(wstring_view spec, wformat_context& context) const {
        if (!is_custom() || !value_.custom_value.object ||
            !value_.custom_value.format) {
            detail::format_fail();
        }
        value_.custom_value.format(value_.custom_value.object, spec, context);
    }

    template<typename Visitor>
    auto visit(Visitor&& visitor) const {
        switch (type_) {
            case detail::wide_format_arg_type::int_type:
                return visitor(value_.int_value);
            case detail::wide_format_arg_type::unsigned_type:
                return visitor(value_.unsigned_value);
            case detail::wide_format_arg_type::long_long_type:
                return visitor(value_.long_long_value);
            case detail::wide_format_arg_type::unsigned_long_long_type:
                return visitor(value_.unsigned_long_long_value);
            case detail::wide_format_arg_type::bool_type:
                return visitor(value_.bool_value);
            case detail::wide_format_arg_type::char_type:
                return visitor(value_.char_value);
            case detail::wide_format_arg_type::float_type:
                return visitor(value_.float_value);
            case detail::wide_format_arg_type::double_type:
                return visitor(value_.double_value);
            case detail::wide_format_arg_type::long_double_type:
                return visitor(value_.long_double_value);
            case detail::wide_format_arg_type::cstring_type:
                return visitor(value_.cstring_value);
            case detail::wide_format_arg_type::string_type:
                return visitor(wstring_view(value_.string_value.data,
                                            value_.string_value.size));
            case detail::wide_format_arg_type::pointer_type:
                return visitor(value_.pointer_value);
            default:
                detail::format_fail();
        }
    }

    format_arg to_narrow(string* string_storage) const {
        switch (type_) {
            case detail::wide_format_arg_type::int_type:
                return format_arg::make(value_.int_value);
            case detail::wide_format_arg_type::unsigned_type:
                return format_arg::make(value_.unsigned_value);
            case detail::wide_format_arg_type::long_long_type:
                return format_arg::make(value_.long_long_value);
            case detail::wide_format_arg_type::unsigned_long_long_type:
                return format_arg::make(value_.unsigned_long_long_value);
            case detail::wide_format_arg_type::bool_type:
                return format_arg::make(value_.bool_value);
            case detail::wide_format_arg_type::char_type:
                if (value_.char_value > 0x7f) detail::format_fail();
                return format_arg::make(static_cast<char>(value_.char_value));
            case detail::wide_format_arg_type::float_type:
                return format_arg::make(value_.float_value);
            case detail::wide_format_arg_type::double_type:
                return format_arg::make(value_.double_value);
            case detail::wide_format_arg_type::long_double_type:
#if defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
    ((__LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384 && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || \
     (__LDBL_MANT_DIG__ == 53 && __LDBL_MAX_EXP__ == 1024))
                return format_arg::make(value_.long_double_value);
#else
                detail::format_fail();
#endif
            case detail::wide_format_arg_type::cstring_type:
                if (!value_.cstring_value) return format_arg::make(
                    static_cast<const char*>(nullptr));
                if (!string_storage) detail::format_fail();
                string_storage->clear();
                while (string_storage->size() < detail::format_max_conversion_size &&
                       value_.cstring_value[string_storage->size()] != L'\0') {
                    const wchar_t character =
                        value_.cstring_value[string_storage->size()];
                    if (character > 0x7f) detail::format_fail();
                    string_storage->push_back(static_cast<char>(character));
                }
                if (string_storage->size() == detail::format_max_conversion_size &&
                    value_.cstring_value[string_storage->size()] != L'\0') {
                    detail::format_fail();
                }
                return format_arg::make(string_view(string_storage->data(),
                                                    string_storage->size()));
            case detail::wide_format_arg_type::string_type:
                if (!string_storage ||
                    (!value_.string_value.data && value_.string_value.size != 0)) {
                    detail::format_fail();
                }
                if (value_.string_value.size > detail::format_max_conversion_size) {
                    detail::format_fail();
                }
                string_storage->clear();
                for (size_t index = 0; index < value_.string_value.size; ++index) {
                    const wchar_t character = value_.string_value.data[index];
                    if (character > 0x7f) detail::format_fail();
                    string_storage->push_back(static_cast<char>(character));
                }
                return format_arg::make(string_view(string_storage->data(),
                                                    string_storage->size()));
            case detail::wide_format_arg_type::pointer_type:
                return format_arg::make(value_.pointer_value);
            default:
                detail::format_fail();
        }
    }
};

/* ═══════════════════════════════════════════════════════════════
 * basic_format_args
 * ═══════════════════════════════════════════════════════════════*/

template<typename Context>
class basic_format_args {
    const basic_format_arg<Context>* args_;
    size_t size_;

public:
    basic_format_args() noexcept : args_(nullptr), size_(0) {}

    template<size_t N>
    basic_format_args(const array<basic_format_arg<Context>, N>& arr)
        : args_(arr.data()), size_(N) {}

    basic_format_args(const basic_format_arg<Context>* args, size_t size)
        : args_(args), size_(size) {}

    basic_format_arg<Context> get(size_t i) const {
        if (i < size_) return args_[i];
        return basic_format_arg<Context>();
    }

    size_t size() const noexcept { return size_; }
};

template<>
class basic_format_args<wformat_context> {
    const wide_format_arg* args_;
    size_t size_;

public:
    basic_format_args() noexcept : args_(nullptr), size_(0) {}

    template<size_t N>
    basic_format_args(const array<wide_format_arg, N>& arr)
        : args_(arr.data()), size_(N) {}

    basic_format_args(const wide_format_arg* args, size_t size)
        : args_(args), size_(size) {}

    wide_format_arg get(size_t index) const {
        if (index < size_) return args_[index];
        return wide_format_arg();
    }

    size_t size() const noexcept { return size_; }
};

using format_args = basic_format_args<format_context>;
using wformat_args = basic_format_args<wformat_context>;

/* ═══════════════════════════════════════════════════════════════
 * make_format_args
 * ═══════════════════════════════════════════════════════════════*/

template<typename Context, typename... Args>
auto make_format_args(Args&&... args) {
    if constexpr (is_same_v<Context, wformat_context>) {
        return array<wide_format_arg, sizeof...(Args)>{
            wide_format_arg::make(std::forward<Args>(args))...
        };
    } else {
        return array<basic_format_arg<Context>, sizeof...(Args)>{
            basic_format_arg<Context>::make(std::forward<Args>(args))...
        };
    }
}

namespace detail {

inline string render_format(string_view fmt, format_args args) {
    if (fmt.size() > format_max_input_size ||
        (!fmt.data() && fmt.size() != 0)) {
        format_fail();
    }
    if (fmt.empty()) return string();

    string result;
    bool output_failed = false;
    format_output_iterator<char> out(result, &output_failed,
                                     format_max_output_size);
    size_t next_arg = 0;
    size_t field_count = 0;
    unsigned indexing = 0; /* 0: unset, 1: automatic, 2: manual */
    const char* const begin = fmt.data();
    const char* const end = begin + fmt.size();
    const char* p = begin;

    const auto dynamic_size = [&](size_t index) -> size_t {
        if (index >= args.size()) format_fail();
        const auto dynamic_arg = args.get(index);
        if (!dynamic_arg) format_fail();

        size_t value = 0u;
        bool accepted = false;
        dynamic_arg.visit([&](auto dynamic_value) {
            using dynamic_type = decay_t<decltype(dynamic_value)>;
            if constexpr (is_same_v<dynamic_type, int> ||
                          is_same_v<dynamic_type, long long>) {
                if (dynamic_value < 0 ||
                    dynamic_value > static_cast<dynamic_type>(format_max_input_size)) {
                    format_fail();
                }
                value = static_cast<size_t>(dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, unsigned> ||
                                 is_same_v<dynamic_type, unsigned long long>) {
                if (dynamic_value >
                    static_cast<dynamic_type>(format_max_input_size)) {
                    format_fail();
                }
                value = static_cast<size_t>(dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, char>) {
                if (dynamic_value < 0) format_fail();
                value = static_cast<size_t>(dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, bool>) {
                value = dynamic_value ? 1u : 0u;
                accepted = true;
            }
            return 0;
        });
        if (!accepted || value > format_max_input_size) format_fail();
        return value;
    };

    while (p != end) {
        if (*p == '{') {
            if ((p + 1) != end && *(p + 1) == '{') {
                *out++ = '{';
                p += 2;
                if (output_failed) format_fail();
                continue;
            }

            ++p;
            if (++field_count > format_max_field_count) format_fail();

            size_t arg_index = 0;
            if (p != end && *p >= '0' && *p <= '9') {
                if (indexing == 1) format_fail();
                indexing = 2;
                do {
                    const size_t digit = static_cast<size_t>(*p - '0');
                    if (arg_index > (static_cast<size_t>(-1) - digit) / 10u) {
                        format_fail();
                    }
                    arg_index = arg_index * 10u + digit;
                    ++p;
                } while (p != end && *p >= '0' && *p <= '9');
            } else {
                if (indexing == 2) format_fail();
                indexing = 1;
                arg_index = next_arg++;
            }

            const char* spec_begin = p;
            if (p != end && *p == ':') {
                spec_begin = ++p;
                while (p != end && *p != '}') {
                    if (static_cast<size_t>(p - spec_begin) >= format_max_spec_size) {
                        format_fail();
                    }
                    if (*p == '{') {
                        ++p;
                        if (p == end) format_fail();
                        if (*p == '}') {
                            ++p;
                            continue;
                        }
                        if (*p < '0' || *p > '9') format_fail();
                        do {
                            ++p;
                        } while (p != end && *p >= '0' && *p <= '9');
                        if (p == end || *p != '}') format_fail();
                        ++p;
                        continue;
                    }
                    ++p;
                }
            } else if (p != end && *p != '}') {
                format_fail();
            }

            if (p == end || *p != '}' || arg_index >= args.size()) {
                format_fail();
            }
            const string_view raw_spec(spec_begin,
                                       static_cast<size_t>(p - spec_begin));
            ++p;

            char resolved_storage[format_max_spec_size + 1u];
            size_t resolved_size = 0u;
            const char* spec_cursor = raw_spec.data();
            const char* const spec_end = spec_cursor + raw_spec.size();
            while (spec_cursor != spec_end) {
                if (*spec_cursor != '{') {
                    if (resolved_size == format_max_spec_size) format_fail();
                    resolved_storage[resolved_size++] = *spec_cursor++;
                    continue;
                }

                ++spec_cursor;
                size_t dynamic_index = 0u;
                if (spec_cursor == spec_end) format_fail();
                if (*spec_cursor == '}') {
                    if (indexing == 2) format_fail();
                    indexing = 1;
                    dynamic_index = next_arg++;
                } else {
                    if (indexing == 1 || *spec_cursor < '0' ||
                        *spec_cursor > '9') {
                        format_fail();
                    }
                    indexing = 2;
                    do {
                        const size_t digit =
                            static_cast<size_t>(*spec_cursor - '0');
                        if (dynamic_index >
                            (static_cast<size_t>(-1) - digit) / 10u) {
                            format_fail();
                        }
                        dynamic_index = dynamic_index * 10u + digit;
                        ++spec_cursor;
                    } while (spec_cursor != spec_end &&
                             *spec_cursor >= '0' && *spec_cursor <= '9');
                    if (spec_cursor == spec_end || *spec_cursor != '}') {
                        format_fail();
                    }
                }

                const size_t dynamic_value = dynamic_size(dynamic_index);
                char reversed[16];
                size_t digits = 0u;
                size_t remaining = dynamic_value;
                do {
                    reversed[digits++] =
                        static_cast<char>('0' + remaining % 10u);
                    remaining /= 10u;
                } while (remaining != 0u);
                if (digits > format_max_spec_size - resolved_size) format_fail();
                while (digits != 0u) {
                    resolved_storage[resolved_size++] = reversed[--digits];
                }
                ++spec_cursor;
            }
            const string_view spec(resolved_storage, resolved_size);

            const auto arg = args.get(arg_index);
            if (!arg) format_fail();
            basic_format_context<format_output_iterator<char>, char>
                context(out, args);
            if (arg.is_custom()) {
                arg.format_custom(spec, context);
            } else {
                arg.visit([&](auto value) {
                    using value_type = decay_t<decltype(value)>;
                    if constexpr (is_same_v<value_type, const char*>) {
                        if (!value) format_fail();
                    }

                    formatter<value_type, char> selected;
                    format_parse_context parse_context(spec);
                    if (selected.parse(parse_context) != parse_context.end()) {
                        format_fail();
                    }
                    selected.format(value, context);
                    return 0;
                });
            }
            if (output_failed) format_fail();
        } else if (*p == '}') {
            if ((p + 1) == end || *(p + 1) != '}') format_fail();
            *out++ = '}';
            p += 2;
            if (output_failed) format_fail();
        } else {
            *out++ = *p++;
            if (output_failed) format_fail();
        }
    }

    return result;
}

inline wstring render_wide_format(wstring_view fmt, wformat_args args) {
    if (fmt.size() > format_max_input_size ||
        (!fmt.data() && fmt.size() != 0) ||
        args.size() > format_max_field_count) {
        format_fail();
    }

    wstring result;
    bool output_failed = false;
    format_output_iterator<wchar_t> out(result, &output_failed,
                                        format_max_output_size);
    size_t next_arg = 0u;
    size_t field_count = 0u;
    unsigned indexing = 0u; /* 0: unset, 1: automatic, 2: manual */
    const wchar_t* const begin = fmt.data();
    const wchar_t* const end = begin + fmt.size();
    const wchar_t* cursor = begin;

    const auto dynamic_size = [&](size_t index) -> size_t {
        if (index >= args.size()) format_fail();
        const wide_format_arg dynamic_arg = args.get(index);
        if (!dynamic_arg) format_fail();

        size_t value = 0u;
        bool accepted = false;
        dynamic_arg.visit([&](auto dynamic_value) {
            using dynamic_type = decay_t<decltype(dynamic_value)>;
            if constexpr (is_same_v<dynamic_type, int> ||
                          is_same_v<dynamic_type, long long>) {
                if (dynamic_value < 0 ||
                    dynamic_value > static_cast<dynamic_type>(
                        format_max_input_size)) {
                    format_fail();
                }
                value = static_cast<size_t>(dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, unsigned> ||
                                 is_same_v<dynamic_type,
                                           unsigned long long>) {
                if (dynamic_value > static_cast<dynamic_type>(
                        format_max_input_size)) {
                    format_fail();
                }
                value = static_cast<size_t>(dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, wchar_t>) {
                if (dynamic_value < 0) format_fail();
                value = static_cast<size_t>(
                    dynamic_value);
                accepted = true;
            } else if constexpr (is_same_v<dynamic_type, bool>) {
                value = dynamic_value ? 1u : 0u;
                accepted = true;
            }
            return 0;
        });
        if (!accepted || value > format_max_input_size) format_fail();
        return value;
    };

    while (cursor != end) {
        if (*cursor == L'{') {
            if ((cursor + 1) != end && *(cursor + 1) == L'{') {
                *out++ = L'{';
                cursor += 2;
                if (output_failed) format_fail();
                continue;
            }

            ++cursor;
            if (++field_count > format_max_field_count) format_fail();

            size_t arg_index = 0u;
            if (cursor != end && *cursor >= L'0' && *cursor <= L'9') {
                if (indexing == 1u) format_fail();
                indexing = 2u;
                do {
                    const size_t digit = static_cast<size_t>(*cursor - L'0');
                    if (arg_index > (static_cast<size_t>(-1) - digit) / 10u) {
                        format_fail();
                    }
                    arg_index = arg_index * 10u + digit;
                    ++cursor;
                } while (cursor != end && *cursor >= L'0' &&
                         *cursor <= L'9');
            } else {
                if (indexing == 2u) format_fail();
                indexing = 1u;
                arg_index = next_arg++;
            }

            const wchar_t* spec_begin = cursor;
            if (cursor != end && *cursor == L':') {
                spec_begin = ++cursor;
                while (cursor != end && *cursor != L'}') {
                    if (static_cast<size_t>(cursor - spec_begin) >=
                        format_max_spec_size) {
                        format_fail();
                    }
                    if (*cursor == L'{') {
                        ++cursor;
                        if (cursor == end) format_fail();
                        if (*cursor == L'}') {
                            ++cursor;
                            continue;
                        }
                        if (*cursor < L'0' || *cursor > L'9') format_fail();
                        do {
                            ++cursor;
                        } while (cursor != end && *cursor >= L'0' &&
                                 *cursor <= L'9');
                        if (cursor == end || *cursor != L'}') format_fail();
                        ++cursor;
                        continue;
                    }
                    ++cursor;
                }
            } else if (cursor != end && *cursor != L'}') {
                format_fail();
            }

            if (cursor == end || *cursor != L'}' || arg_index >= args.size()) {
                format_fail();
            }
            const wstring_view raw_spec(
                spec_begin, static_cast<size_t>(cursor - spec_begin));
            ++cursor;

            wchar_t resolved_storage[format_max_spec_size + 1u];
            size_t resolved_size = 0u;
            const wchar_t* spec_cursor = raw_spec.data();
            const wchar_t* const spec_end = spec_cursor + raw_spec.size();
            while (spec_cursor != spec_end) {
                if (*spec_cursor != L'{') {
                    if (resolved_size == format_max_spec_size) format_fail();
                    resolved_storage[resolved_size++] = *spec_cursor++;
                    continue;
                }

                ++spec_cursor;
                size_t dynamic_index = 0u;
                if (spec_cursor == spec_end) format_fail();
                if (*spec_cursor == L'}') {
                    if (indexing == 2u) format_fail();
                    indexing = 1u;
                    dynamic_index = next_arg++;
                } else {
                    if (indexing == 1u || *spec_cursor < L'0' ||
                        *spec_cursor > L'9') {
                        format_fail();
                    }
                    indexing = 2u;
                    do {
                        const size_t digit =
                            static_cast<size_t>(*spec_cursor - L'0');
                        if (dynamic_index >
                            (static_cast<size_t>(-1) - digit) / 10u) {
                            format_fail();
                        }
                        dynamic_index = dynamic_index * 10u + digit;
                        ++spec_cursor;
                    } while (spec_cursor != spec_end &&
                             *spec_cursor >= L'0' && *spec_cursor <= L'9');
                    if (spec_cursor == spec_end || *spec_cursor != L'}') {
                        format_fail();
                    }
                }

                const size_t dynamic_value = dynamic_size(dynamic_index);
                wchar_t reversed[16];
                size_t digits = 0u;
                size_t remaining = dynamic_value;
                do {
                    reversed[digits++] =
                        static_cast<wchar_t>(L'0' + remaining % 10u);
                    remaining /= 10u;
                } while (remaining != 0u);
                if (digits > format_max_spec_size - resolved_size) format_fail();
                while (digits != 0u) {
                    resolved_storage[resolved_size++] = reversed[--digits];
                }
                ++spec_cursor;
            }

            const wide_format_arg arg = args.get(arg_index);
            if (!arg) format_fail();
            if (arg.is_custom()) {
                basic_format_context<format_output_iterator<wchar_t>, wchar_t>
                    context(out, args);
                arg.format_custom(
                    wstring_view(resolved_storage, resolved_size), context);
            } else {
                basic_format_context<format_output_iterator<wchar_t>, wchar_t>
                    context(out, args);
                arg.visit([&](auto value) {
                    using value_type = decay_t<decltype(value)>;
                    if constexpr (is_same_v<value_type, const wchar_t*>) {
                        if (!value) format_fail();
                    }
                    formatter<value_type, wchar_t> selected;
                    wformat_parse_context parse_context(
                        wstring_view(resolved_storage, resolved_size));
                    if (selected.parse(parse_context) != parse_context.end()) {
                        format_fail();
                    }
                    selected.format(value, context);
                    return 0;
                });
            }
        } else if (*cursor == L'}') {
            if ((cursor + 1) == end || *(cursor + 1) != L'}') format_fail();
            *out++ = L'}';
            cursor += 2;
            if (output_failed) format_fail();
        } else {
            if (*cursor > 0x7f) format_fail();
            *out++ = *cursor++;
            if (output_failed) format_fail();
        }
    }
    return result;
}

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * vformat_to - フォーマットの実装
 * ═══════════════════════════════════════════════════════════════*/

template<typename OutputIt>
OutputIt vformat_to(OutputIt out, string_view fmt, format_args args) {
    const string rendered = detail::render_format(fmt, args);
    for (char c : rendered) *out++ = c;
    return out;
}

template<typename OutputIt>
struct format_to_n_result {
    OutputIt out;
    size_t size;
};

/* ═══════════════════════════════════════════════════════════════
 * vformat
 * ═══════════════════════════════════════════════════════════════*/

inline string vformat(string_view fmt, format_args args) {
    return detail::render_format(fmt, args);
}

/* ═══════════════════════════════════════════════════════════════
 * format
 * ═══════════════════════════════════════════════════════════════*/

template<typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, string_view>, int> = 0>
string format(FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<format_context>(std::forward<Args>(args)...);
    return vformat(fmt, format_args(arg_array.data(), arg_array.size()));
}

template<typename... Args>
string format(format_string<Args...> fmt, Args&&... args) {
    return format(fmt.get(), std::forward<Args>(args)...);
}

/* Prefer the compile-time carrier for an actual string literal.  The
 * string_view overload remains available for callers that intentionally defer
 * validation (for example, data received at runtime). */
template<typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, string_view>, int> = 0>
size_t formatted_size(FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<format_context>(std::forward<Args>(args)...);
    return detail::render_format(
        fmt, format_args(arg_array.data(), arg_array.size())).size();
}

template<typename... Args>
size_t formatted_size(format_string<Args...> fmt, Args&&... args) {
    return formatted_size(fmt.get(), std::forward<Args>(args)...);
}

/* ═══════════════════════════════════════════════════════════════
 * format_to
 * ═══════════════════════════════════════════════════════════════*/

template<typename OutputIt, typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, string_view>, int> = 0>
OutputIt format_to(OutputIt out, FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<format_context>(std::forward<Args>(args)...);
    return vformat_to(out, fmt, format_args(arg_array.data(), arg_array.size()));
}

template<typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, format_string<Args...> fmt, Args&&... args) {
    return format_to(out, fmt.get(), std::forward<Args>(args)...);
}

template<typename OutputIt, typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, string_view>, int> = 0>
format_to_n_result<OutputIt> format_to_n(OutputIt out, size_t n,
                                          FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<format_context>(std::forward<Args>(args)...);
    const string rendered = detail::render_format(
        fmt, format_args(arg_array.data(), arg_array.size()));
    const size_t written = rendered.size() < n ? rendered.size() : n;
    for (size_t index = 0u; index < written; ++index) *out++ = rendered[index];
    return {out, rendered.size()};
}

template<typename OutputIt, typename... Args>
format_to_n_result<OutputIt> format_to_n(OutputIt out, size_t n,
                                          format_string<Args...> fmt,
                                          Args&&... args) {
    return format_to_n(out, n, fmt.get(), std::forward<Args>(args)...);
}

inline wstring vformat(wstring_view fmt, wformat_args args) {
    return detail::render_wide_format(fmt, args);
}

template<typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, wstring_view>, int> = 0>
wstring format(FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<wformat_context>(
        std::forward<Args>(args)...);
    return vformat(fmt, wformat_args(arg_array.data(), arg_array.size()));
}

template<typename... Args>
wstring format(wformat_string<Args...> fmt, Args&&... args) {
    return format(fmt.get(), std::forward<Args>(args)...);
}

template<typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, wstring_view>, int> = 0>
size_t formatted_size(FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<wformat_context>(
        std::forward<Args>(args)...);
    return vformat(fmt, wformat_args(arg_array.data(), arg_array.size())).size();
}

template<typename... Args>
size_t formatted_size(wformat_string<Args...> fmt, Args&&... args) {
    return formatted_size(fmt.get(), std::forward<Args>(args)...);
}

template<typename OutputIt, typename... Args>
OutputIt vformat_to(OutputIt out, wstring_view fmt, wformat_args args) {
    const wstring rendered = detail::render_wide_format(fmt, args);
    for (wchar_t character : rendered) *out++ = character;
    return out;
}

template<typename OutputIt, typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, wstring_view>, int> = 0>
OutputIt format_to(OutputIt out, FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<wformat_context>(
        std::forward<Args>(args)...);
    return vformat_to(out, fmt,
                      wformat_args(arg_array.data(), arg_array.size()));
}

template<typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, wformat_string<Args...> fmt, Args&&... args) {
    return format_to(out, fmt.get(), std::forward<Args>(args)...);
}

template<typename OutputIt, typename FormatArg, typename... Args,
         enable_if_t<is_same_v<decay_t<FormatArg>, wstring_view>, int> = 0>
format_to_n_result<OutputIt> format_to_n(OutputIt out, size_t n,
                                          FormatArg&& fmt, Args&&... args) {
    auto arg_array = make_format_args<wformat_context>(
        std::forward<Args>(args)...);
    const wstring rendered = vformat(
        fmt, wformat_args(arg_array.data(), arg_array.size()));
    const size_t written = rendered.size() < n ? rendered.size() : n;
    for (size_t index = 0; index < written; ++index) *out++ = rendered[index];
    return {out, rendered.size()};
}

template<typename OutputIt, typename... Args>
format_to_n_result<OutputIt> format_to_n(OutputIt out, size_t n,
                                          wformat_string<Args...> fmt,
                                          Args&&... args) {
    return format_to_n(out, n, fmt.get(), std::forward<Args>(args)...);
}

} /* namespace std */

#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_FORMAT_H */
