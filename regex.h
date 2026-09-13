/*
 * RinOS C++ <regex> ✿
 * 正規表現 - bounded ECMAScript subset
 */

#ifndef RINCXX_REGEX_H
#define RINCXX_REGEX_H

#include "cstddef.h"
#include "compare.h"
#include "initializer_list.h"
#include "rincxx.h"
#include "iterator.h"
#include "locale.h"
#include "ostream.h"
#include "string.h"
#include "stdexcept.h"
#include "vector.h"

#ifdef __cplusplus

namespace std {

/* ═══════════════════════════════════════════════════════════════
 * regex_constants - 正規表現フラグ
 * ═══════════════════════════════════════════════════════════════*/

namespace regex_constants {

enum syntax_option_type : unsigned int {};
constexpr syntax_option_type icase = static_cast<syntax_option_type>(1u);
constexpr syntax_option_type nosubs = static_cast<syntax_option_type>(2u);
constexpr syntax_option_type optimize = static_cast<syntax_option_type>(4u);
constexpr syntax_option_type collate = static_cast<syntax_option_type>(8u);
constexpr syntax_option_type ECMAScript = static_cast<syntax_option_type>(16u);
constexpr syntax_option_type basic = static_cast<syntax_option_type>(32u);
constexpr syntax_option_type extended = static_cast<syntax_option_type>(64u);
constexpr syntax_option_type awk = static_cast<syntax_option_type>(128u);
constexpr syntax_option_type grep = static_cast<syntax_option_type>(256u);
constexpr syntax_option_type egrep = static_cast<syntax_option_type>(512u);
constexpr syntax_option_type multiline = static_cast<syntax_option_type>(1024u);

constexpr syntax_option_type operator|(syntax_option_type left,
                                       syntax_option_type right) {
    return static_cast<syntax_option_type>(
        static_cast<unsigned int>(left) | static_cast<unsigned int>(right));
}
constexpr syntax_option_type operator&(syntax_option_type left,
                                       syntax_option_type right) {
    return static_cast<syntax_option_type>(
        static_cast<unsigned int>(left) & static_cast<unsigned int>(right));
}
constexpr syntax_option_type operator^(syntax_option_type left,
                                       syntax_option_type right) {
    return static_cast<syntax_option_type>(
        static_cast<unsigned int>(left) ^ static_cast<unsigned int>(right));
}
constexpr syntax_option_type operator~(syntax_option_type value) {
    return static_cast<syntax_option_type>(~static_cast<unsigned int>(value));
}
inline syntax_option_type& operator|=(syntax_option_type& left,
                                     syntax_option_type right) {
    return left = left | right;
}
inline syntax_option_type& operator&=(syntax_option_type& left,
                                     syntax_option_type right) {
    return left = left & right;
}
inline syntax_option_type& operator^=(syntax_option_type& left,
                                     syntax_option_type right) {
    return left = left ^ right;
}

enum match_flag_type : unsigned int {};
constexpr match_flag_type match_default = static_cast<match_flag_type>(0u);
constexpr match_flag_type match_not_bol = static_cast<match_flag_type>(1u);
constexpr match_flag_type match_not_eol = static_cast<match_flag_type>(2u);
constexpr match_flag_type match_not_bow = static_cast<match_flag_type>(4u);
constexpr match_flag_type match_not_eow = static_cast<match_flag_type>(8u);
constexpr match_flag_type match_any = static_cast<match_flag_type>(16u);
constexpr match_flag_type match_not_null = static_cast<match_flag_type>(32u);
constexpr match_flag_type match_continuous = static_cast<match_flag_type>(64u);
constexpr match_flag_type match_prev_avail = static_cast<match_flag_type>(128u);
constexpr match_flag_type format_default = static_cast<match_flag_type>(0u);
constexpr match_flag_type format_sed = static_cast<match_flag_type>(256u);
constexpr match_flag_type format_no_copy = static_cast<match_flag_type>(512u);
constexpr match_flag_type format_first_only = static_cast<match_flag_type>(1024u);

constexpr match_flag_type operator|(match_flag_type left,
                                    match_flag_type right) {
    return static_cast<match_flag_type>(
        static_cast<unsigned int>(left) | static_cast<unsigned int>(right));
}
constexpr match_flag_type operator&(match_flag_type left,
                                    match_flag_type right) {
    return static_cast<match_flag_type>(
        static_cast<unsigned int>(left) & static_cast<unsigned int>(right));
}
constexpr match_flag_type operator^(match_flag_type left,
                                    match_flag_type right) {
    return static_cast<match_flag_type>(
        static_cast<unsigned int>(left) ^ static_cast<unsigned int>(right));
}
constexpr match_flag_type operator~(match_flag_type value) {
    return static_cast<match_flag_type>(~static_cast<unsigned int>(value));
}
inline match_flag_type& operator|=(match_flag_type& left,
                                  match_flag_type right) {
    return left = left | right;
}
inline match_flag_type& operator&=(match_flag_type& left,
                                  match_flag_type right) {
    return left = left & right;
}
inline match_flag_type& operator^=(match_flag_type& left,
                                  match_flag_type right) {
    return left = left ^ right;
}

using error_type = int;
constexpr error_type error_collate = 0;
constexpr error_type error_ctype = 1;
constexpr error_type error_escape = 2;
constexpr error_type error_backref = 3;
constexpr error_type error_brack = 4;
constexpr error_type error_paren = 5;
constexpr error_type error_brace = 6;
constexpr error_type error_badbrace = 7;
constexpr error_type error_range = 8;
constexpr error_type error_space = 9;
constexpr error_type error_badrepeat = 10;
constexpr error_type error_complexity = 11;
constexpr error_type error_stack = 12;

} /* namespace regex_constants */

/* ═══════════════════════════════════════════════════════════════
 * regex_error - 正規表現例外
 * ═══════════════════════════════════════════════════════════════*/

class regex_error : public runtime_error {
    regex_constants::error_type error_;
public:
    explicit regex_error(regex_constants::error_type e)
        : runtime_error("regex_error"), error_(e) {}

    regex_constants::error_type code() const noexcept { return error_; }
};

namespace __regex_detail {

static constexpr size_t maximum_atom_count = 256u;
/* Interval repetition is represented by a counter, not a recursive frame.
 * Keep the counter wide enough for the standard's decimal interval grammar;
 * the matcher still has the shared operation budget below, so a hostile
 * input cannot turn an oversized interval into unbounded work. */
static constexpr size_t maximum_repeat = 65535u;
static constexpr size_t maximum_complex_group_repeat = 256u;
/* Group quantifiers use the same decimal interval range as plain atoms.  The
 * deterministic single-unit path is iterative; complex groups retain the
 * independent nesting/operation bounds in the fallback matcher. */
static constexpr size_t maximum_group_repeat = maximum_repeat;
/* Complex groups still use the recursive continuation fallback.  Keep its
 * historical stack bound until all alternatives have an iterative frame. */
static constexpr size_t maximum_recursive_group_repeat = 256u;
static constexpr size_t maximum_match_operations = 65536u;
/* The public match_results surface is bounded, but supports the complete
 * two-decimal backreference/replacement range used by this implementation.
 * Keep parser, capture snapshots, and public submatch storage on one limit. */
static constexpr size_t maximum_capture_count = 64u;
static constexpr size_t maximum_group_nesting = 16u;
static constexpr size_t maximum_alternative_count = 64u;
/* Fixed-width lookbehind is charged against the per-candidate matcher
 * budget.  Keep enough headroom for both the rewind and the assertion's
 * forward scan while removing the old 4,096-code-unit artificial cap. */
static constexpr size_t maximum_lookbehind_width = 16384u;
/* A traits-provided collating element is a bounded sequence, not necessarily
 * one code unit.  Keep the sequence small enough that matching and primary
 * key construction remain allocation-bounded. */
static constexpr size_t maximum_collating_element_width = 64u;

/* The bounded matcher deliberately stops before its operation budget can
 * wrap.  A budget exhaustion is observable standard regex behavior, not an
 * ordinary non-match: hosted exception-enabled callers receive the required
 * error_complexity diagnostic, while exception-disabled products keep the
 * existing fail-closed non-match contract. */
[[noreturn]] inline void complexity_fail() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw regex_error(regex_constants::error_complexity);
#else
    __builtin_trap();
#endif
}

enum atom_kind {
    atom_literal,
    atom_dot,
    atom_class,
    atom_digit,
    atom_not_digit,
    atom_space,
    atom_not_space,
    atom_word,
    atom_not_word,
    atom_group,
    atom_backreference,
    atom_bol,
    atom_eol,
    atom_word_boundary,
    atom_not_word_boundary,
    atom_positive_lookahead,
    atom_negative_lookahead,
    atom_positive_lookbehind,
    atom_negative_lookbehind
};

template<typename CharT>
struct atom {
    atom_kind kind = atom_literal;
    CharT literal = CharT();
    size_t class_begin = 0u;
    size_t class_end = 0u;
    bool class_negated = false;
    size_t group_begin = 0u;
    size_t group_end = 0u;
    size_t capture_index = 0u;
    size_t capture_begin = 1u;
    size_t capture_end = 0u;
    bool capturing = false;
    size_t next = 0u;
    size_t minimum = 1u;
    size_t maximum = 1u;
    bool quantified = false;
    bool greedy = true;
};

template<typename CharT>
bool ascii_alpha(CharT value) {
    return (value >= CharT('a') && value <= CharT('z')) ||
           (value >= CharT('A') && value <= CharT('Z'));
}

template<typename CharT>
bool ascii_digit(CharT value) {
    return value >= CharT('0') && value <= CharT('9');
}

template<typename CharT, typename Traits>
bool exact_escape_value(const basic_string<CharT>& pattern, size_t begin,
                        size_t end, size_t digits, const Traits& traits,
                        unsigned long& value) {
    if (begin + digits > end) return false;
    value = 0ul;
    for (size_t offset = 0u; offset < digits; ++offset) {
        int converted = traits.value(pattern[begin + offset], 16);
        if (converted < 0 || converted >= 16) return false;
        value = value * 16ul + static_cast<unsigned long>(converted);
    }
    return true;
}

template<typename CharT, typename Traits>
bool legacy_octal_escape_value(const basic_string<CharT>& pattern,
                               size_t index, size_t end,
                               const Traits& traits, unsigned long& value,
                               size_t& next);

template<typename CharT, typename Traits>
bool escaped_unit(const basic_string<CharT>& pattern, size_t index,
                  size_t end, bool in_class, atom_kind& kind,
                  CharT& literal, size_t& next, const Traits& traits) {
    if (index >= end || pattern[index] != CharT('\\') || index + 1u >= end)
        return false;
    CharT value = pattern[index + 1u];
    next = index + 2u;
    switch (value) {
    case CharT('d'): kind = atom_digit; return true;
    case CharT('D'): kind = atom_not_digit; return true;
    case CharT('s'): kind = atom_space; return true;
    case CharT('S'): kind = atom_not_space; return true;
    case CharT('w'): kind = atom_word; return true;
    case CharT('W'): kind = atom_not_word; return true;
    case CharT('n'): literal = CharT('\n'); break;
    case CharT('r'): literal = CharT('\r'); break;
    case CharT('t'): literal = CharT('\t'); break;
    case CharT('f'): literal = CharT('\f'); break;
    case CharT('v'): literal = CharT('\v'); break;
    case CharT('0'): {
        unsigned long converted = 0ul;
        if (!legacy_octal_escape_value(pattern, index, end, traits,
                                       converted, next))
            return false;
        literal = static_cast<CharT>(converted);
        break;
    }
    case CharT('c'):
        if (index + 2u >= end || !ascii_alpha(pattern[index + 2u]))
            return false;
        literal = static_cast<CharT>(
            static_cast<unsigned long>(pattern[index + 2u]) & 0x1ful);
        next = index + 3u;
        break;
    case CharT('x'):
    case CharT('u'): {
        size_t digits = value == CharT('x') ? 2u : 4u;
        unsigned long converted = 0ul;
        if (!exact_escape_value(pattern, index + 2u, end, digits, traits,
                                converted))
            return false;
        const unsigned long maximum = sizeof(CharT) == 1u
            ? 0xfful : (sizeof(CharT) == 2u ? 0xfffful : 0xfffffffful);
        if (converted > maximum) return false;
        literal = static_cast<CharT>(converted);
        next = index + 2u + digits;
        break;
    }
    case CharT('b'):
        if (!in_class) {
            kind = atom_word_boundary;
            return true;
        }
        literal = CharT('\b');
        break;
    case CharT('B'):
        if (in_class) return false;
        kind = atom_not_word_boundary;
        return true;
    default:
        if (ascii_digit(value)) return false;
        literal = value;
        break;
    }
    kind = atom_literal;
    return true;
}

template<typename CharT, typename Traits>
bool class_unit(const basic_string<CharT>& pattern, size_t index,
                size_t end, atom_kind& kind, CharT& literal,
                size_t& next, const Traits& traits) {
    if (index >= end || pattern[index] == CharT(']')) return false;
    if (pattern[index] == CharT('\\'))
        return escaped_unit(pattern, index, end, true, kind, literal, next,
                            traits);
    kind = atom_literal;
    literal = pattern[index];
    next = index + 1u;
    return true;
}

enum class_special_kind {
    class_special_none,
    class_special_named,
    class_special_collating,
    class_special_equivalence
};

struct class_special {
    class_special_kind kind = class_special_none;
    size_t name_begin = 0u;
    size_t name_end = 0u;
    size_t next = 0u;
};

template<typename CharT>
bool parse_class_special(const basic_string<CharT>& pattern, size_t index,
                         size_t end, class_special& output) {
    output = class_special();
    if (index + 3u >= end || pattern[index] != CharT('[')) return false;
    CharT marker = pattern[index + 1u];
    if (marker == CharT(':')) output.kind = class_special_named;
    else if (marker == CharT('.')) output.kind = class_special_collating;
    else if (marker == CharT('=')) output.kind = class_special_equivalence;
    else return false;

    output.name_begin = index + 2u;
    size_t cursor = output.name_begin;
    while (cursor + 1u < end) {
        if (pattern[cursor] == marker &&
            pattern[cursor + 1u] == CharT(']')) {
            if (cursor == output.name_begin) return false;
            output.name_end = cursor;
            output.next = cursor + 2u;
            return true;
        }
        ++cursor;
    }
    return false;
}

template<typename CharT, typename Traits>
bool supported_class_special(const basic_string<CharT>& pattern,
                             const class_special& special,
                             const Traits& traits) {
    auto first = pattern.begin() + special.name_begin;
    auto last = pattern.begin() + special.name_end;
    if (special.kind == class_special_named) {
        using class_type = typename Traits::char_class_type;
        return traits.lookup_classname(first, last, false) != class_type() ||
               traits.lookup_classname(first, last, true) != class_type();
    }
    typename Traits::string_type collating =
        traits.lookup_collatename(first, last);
    if (collating.empty() ||
        collating.size() > maximum_collating_element_width)
        return false;
    return special.kind != class_special_equivalence ||
           !traits.transform_primary(collating.begin(),
                                     collating.end()).empty();
}

template<typename CharT, typename Traits>
bool class_special_is_multicode(const basic_string<CharT>& pattern,
                                const class_special& special,
                                const Traits& traits) {
    if (special.kind == class_special_named) return false;
    auto first = pattern.begin() + special.name_begin;
    auto last = pattern.begin() + special.name_end;
    typename Traits::string_type collating =
        traits.lookup_collatename(first, last);
    return collating.size() > 1u &&
           collating.size() <= maximum_collating_element_width;
}

template<typename CharT, typename Traits>
bool class_special_value(const basic_string<CharT>& pattern,
                         const class_special& special,
                         const Traits& traits, CharT& value) {
    if (special.kind == class_special_named) return false;
    auto first = pattern.begin() + special.name_begin;
    auto last = pattern.begin() + special.name_end;
    typename Traits::string_type collating =
        traits.lookup_collatename(first, last);
    if (collating.size() != 1u) return false;
    value = collating[0];
    return true;
}

template<typename CharT, typename Traits>
bool class_range_endpoint(const basic_string<CharT>& pattern,
                          size_t index, size_t end, const Traits& traits,
                          CharT& literal, size_t& next) {
    class_special special;
    if (parse_class_special(pattern, index, end, special)) {
        if (!supported_class_special(pattern, special, traits) ||
            !class_special_value(pattern, special, traits, literal))
            return false;
        next = special.next;
        return true;
    }
    atom_kind kind;
    return class_unit(pattern, index, end, kind, literal, next, traits) &&
           kind == atom_literal;
}

template<typename CharT, typename Traits>
bool class_contains_multicode(const basic_string<CharT>& pattern,
                              size_t begin, size_t end,
                              const Traits& traits) {
    size_t cursor = begin;
    while (cursor < end) {
        class_special special;
        if (parse_class_special(pattern, cursor, end, special)) {
            if (class_special_is_multicode(pattern, special, traits))
                return true;
            cursor = special.next;
            continue;
        }
        atom_kind kind;
        CharT literal = CharT();
        size_t next = 0u;
        if (!class_unit(pattern, cursor, end, kind, literal, next, traits))
            return false;
        cursor = next;
    }
    return false;
}

template<typename CharT, typename Traits>
bool class_single_multicode_width(const basic_string<CharT>& pattern,
                                  size_t begin, size_t end,
                                  const Traits& traits, size_t& width) {
    size_t cursor = begin;
    bool found = false;
    width = 0u;
    while (cursor < end) {
        class_special special;
        if (!parse_class_special(pattern, cursor, end, special) ||
            special.kind == class_special_named || found)
            return false;
        auto first = pattern.begin() + special.name_begin;
        auto last = pattern.begin() + special.name_end;
        typename Traits::string_type element =
            traits.lookup_collatename(first, last);
        if (element.size() <= 1u ||
            element.size() > maximum_collating_element_width)
            return false;
        width = element.size();
        found = true;
        cursor = special.next;
    }
    return found;
}

template<typename CharT, typename Traits>
bool class_range_element(const basic_string<CharT>& pattern, size_t index,
                         size_t end, const Traits& traits,
                         typename Traits::string_type& element,
                         size_t& next) {
    class_special special;
    if (parse_class_special(pattern, index, end, special)) {
        if (!supported_class_special(pattern, special, traits) ||
            special.kind == class_special_named)
            return false;
        auto first = pattern.begin() + special.name_begin;
        auto last = pattern.begin() + special.name_end;
        element = traits.lookup_collatename(first, last);
        if (element.empty() ||
            element.size() > maximum_collating_element_width)
            return false;
        next = special.next;
        return true;
    }
    atom_kind kind;
    CharT literal = CharT();
    if (!class_unit(pattern, index, end, kind, literal, next, traits) ||
        kind != atom_literal)
        return false;
    element.assign(1u, literal);
    return true;
}

template<typename CharT, typename Traits>
bool legacy_octal_escape_value(const basic_string<CharT>& pattern,
                               size_t index, size_t end,
                               const Traits& traits, unsigned long& value,
                               size_t& next) {
    if (index + 1u >= end || pattern[index] != CharT('\\') ||
        pattern[index + 1u] != CharT('0'))
        return false;
    value = 0ul;
    size_t cursor = index + 1u;
    size_t digits = 0u;
    while (cursor < end && digits < 3u) {
        int converted = traits.value(pattern[cursor], 8);
        if (converted < 0 || converted >= 8) break;
        value = value * 8ul + static_cast<unsigned long>(converted);
        ++cursor;
        ++digits;
    }
    if (digits == 0u || (cursor < end && ascii_digit(pattern[cursor])))
        return false;
    const unsigned long maximum = sizeof(CharT) == 1u
        ? 0xfful : (sizeof(CharT) == 2u ? 0xfffful : 0xfffffffful);
    if (value > maximum) return false;
    next = cursor;
    return true;
}

template<typename CharT>
bool skip_character_class(const basic_string<CharT>& pattern, size_t index,
                          size_t end, size_t& next) {
    if (index >= end || pattern[index] != CharT('[')) return false;
    size_t cursor = index + 1u;
    while (cursor < end) {
        if (pattern[cursor] == CharT('\\')) {
            if (cursor + 1u >= end) return false;
            cursor += 2u;
        } else if (pattern[cursor] == CharT('[')) {
            class_special special;
            if (parse_class_special(pattern, cursor, end, special))
                cursor = special.next;
            else
                ++cursor;
        } else if (pattern[cursor] == CharT(']')) {
            next = cursor + 1u;
            return true;
        } else {
            ++cursor;
        }
    }
    return false;
}

template<typename CharT>
bool find_group_close(const basic_string<CharT>& pattern, size_t index,
                      size_t end, size_t& close) {
    if (index >= end || pattern[index] != CharT('(')) return false;
    size_t depth = 1u;
    size_t cursor = index + 1u;
    while (cursor < end) {
        if (pattern[cursor] == CharT('\\')) {
            if (cursor + 1u >= end) return false;
            cursor += 2u;
        } else if (pattern[cursor] == CharT('[')) {
            if (!skip_character_class(pattern, cursor, end, cursor))
                return false;
        } else if (pattern[cursor] == CharT('(')) {
            ++depth;
            ++cursor;
        } else if (pattern[cursor] == CharT(')')) {
            if (--depth == 0u) {
                close = cursor;
                return true;
            }
            ++cursor;
        } else {
            ++cursor;
        }
    }
    return false;
}

template<typename CharT>
size_t captures_before(const basic_string<CharT>& pattern, size_t end) {
    size_t count = 0u;
    size_t cursor = 0u;
    while (cursor < end) {
        if (pattern[cursor] == CharT('\\')) {
            cursor += cursor + 1u < end ? 2u : 1u;
        } else if (pattern[cursor] == CharT('[')) {
            size_t next;
            if (!skip_character_class(pattern, cursor, end, next)) return count;
            cursor = next;
        } else {
            if (pattern[cursor] == CharT('(') &&
                !(cursor + 1u < end && pattern[cursor + 1u] == CharT('?')))
                ++count;
            ++cursor;
        }
    }
    return count;
}

template<typename CharT, typename Traits>
bool parse_bounded_decimal(const basic_string<CharT>& pattern,
                           size_t& index, size_t end, size_t& value,
                           const Traits& traits) {
    size_t begin = index;
    value = 0u;
    while (index < end && ascii_digit(pattern[index])) {
        int converted = traits.value(pattern[index], 10);
        if (converted < 0 || converted >= 10) return false;
        size_t digit = static_cast<size_t>(converted);
        if (value > (maximum_repeat - digit) / 10u) return false;
        value = value * 10u + digit;
        ++index;
    }
    return index != begin;
}

template<typename CharT, typename Traits>
bool parse_atom(const basic_string<CharT>& pattern, size_t index,
                size_t end, atom<CharT>& output, const Traits& traits) {
    if (index >= end) return false;
    output = atom<CharT>();
    CharT value = pattern[index];
    if (value == CharT('\\')) {
        if (index + 1u < end && pattern[index + 1u] >= CharT('1') &&
            pattern[index + 1u] <= CharT('9')) {
            output.kind = atom_backreference;
            size_t cursor = index + 1u;
            size_t capture_index = 0u;
            while (cursor < end && ascii_digit(pattern[cursor])) {
                int converted = traits.value(pattern[cursor], 10);
                if (converted < 0 || converted >= 10) return false;
                size_t digit = static_cast<size_t>(converted);
                if (capture_index >
                    (maximum_capture_count - digit) / 10u)
                    return false;
                capture_index = capture_index * 10u + digit;
                ++cursor;
            }
            if (capture_index == 0u ||
                capture_index > maximum_capture_count)
                return false;
            output.capture_index = capture_index;
            output.next = cursor;
        } else if (!escaped_unit(pattern, index, end, false, output.kind,
                                 output.literal, output.next, traits)) {
            return false;
        }
    } else if (value == CharT('.')) {
        output.kind = atom_dot;
        output.next = index + 1u;
    } else if (value == CharT('[')) {
        size_t cursor = index + 1u;
        bool any = false;
        output.kind = atom_class;
        if (cursor < end && pattern[cursor] == CharT('^')) {
            output.class_negated = true;
            ++cursor;
        }
        output.class_begin = cursor;
        while (cursor < end && pattern[cursor] != CharT(']')) {
            class_special special;
            if (parse_class_special(pattern, cursor, end, special)) {
                if (!supported_class_special(pattern, special, traits))
                    return false;
                any = true;
                cursor = special.next;
                if (cursor < end && pattern[cursor] == CharT('-') &&
                    cursor + 1u < end &&
                    pattern[cursor + 1u] != CharT(']')) {
                    if (class_special_is_multicode(pattern, special, traits)) {
                        typename Traits::string_type first_element =
                            traits.lookup_collatename(
                                pattern.begin() + special.name_begin,
                                pattern.begin() + special.name_end);
                        typename Traits::string_type last_element;
                        size_t after_last = 0u;
                        if ((special.kind != class_special_collating &&
                             special.kind != class_special_equivalence) ||
                            !class_range_element(pattern, cursor + 1u, end,
                                                 traits, last_element,
                                                 after_last) ||
                            last_element.size() != first_element.size() ||
                            (special.kind == class_special_collating
                                 ? traits.transform(first_element.begin(),
                                                    first_element.end())
                                 : traits.transform_primary(
                                       first_element.begin(),
                                       first_element.end())) >
                                (special.kind == class_special_collating
                                     ? traits.transform(last_element.begin(),
                                                        last_element.end())
                                     : traits.transform_primary(
                                           last_element.begin(),
                                           last_element.end())))
                            return false;
                        cursor = after_last;
                        continue;
                    }
                    CharT first_literal = CharT();
                    CharT last_literal = CharT();
                    size_t after_last = 0u;
                    if (!class_special_value(pattern, special, traits,
                                             first_literal) ||
                        !class_range_endpoint(
                            pattern, cursor + 1u, end, traits,
                            last_literal, after_last) ||
                        last_literal < first_literal)
                        return false;
                    cursor = after_last;
                }
                continue;
            }
            atom_kind first_kind;
            CharT first_literal = CharT();
            size_t after_first;
            if (!class_unit(pattern, cursor, end, first_kind,
                            first_literal, after_first, traits)) return false;
            any = true;
            cursor = after_first;
            if (first_kind == atom_literal && cursor < end &&
                pattern[cursor] == CharT('-') && cursor + 1u < end &&
                pattern[cursor + 1u] != CharT(']')) {
                CharT last_literal = CharT();
                size_t after_last;
                if (!class_range_endpoint(
                        pattern, cursor + 1u, end, traits,
                        last_literal, after_last) ||
                    last_literal < first_literal)
                    return false;
                cursor = after_last;
            }
        }
        if (!any || cursor >= end || pattern[cursor] != CharT(']'))
            return false;
        output.class_end = cursor;
        if (output.class_negated &&
            class_contains_multicode(pattern, output.class_begin,
                                      output.class_end, traits))
            return false;
        output.next = cursor + 1u;
    } else if (value == CharT('(')) {
        size_t close;
        if (!find_group_close(pattern, index, end, close)) return false;
        size_t capture_before = captures_before(pattern, index);
        output.kind = atom_group;
        output.group_begin = index + 1u;
        output.group_end = close;
        output.capturing = true;
        if (output.group_begin < close &&
            pattern[output.group_begin] == CharT('?')) {
            if (output.group_begin + 1u >= close) return false;
            CharT marker = pattern[output.group_begin + 1u];
            if (marker == CharT('=')) {
                output.kind = atom_positive_lookahead;
                output.group_begin += 2u;
            } else if (marker == CharT('!')) {
                output.kind = atom_negative_lookahead;
                output.group_begin += 2u;
            } else if (marker == CharT(':')) {
                output.group_begin += 2u;
            } else if (marker == CharT('<')) {
                if (output.group_begin + 2u >= close) return false;
                CharT direction = pattern[output.group_begin + 2u];
                if (direction == CharT('='))
                    output.kind = atom_positive_lookbehind;
                else if (direction == CharT('!'))
                    output.kind = atom_negative_lookbehind;
                else
                    return false;
                output.group_begin += 3u;
            } else {
                return false;
            }
            output.capturing = false;
        }
        if (output.capturing) {
            output.capture_index = capture_before + 1u;
            if (output.capture_index > maximum_capture_count) return false;
        }
        output.capture_begin = capture_before + 1u;
        output.capture_end = captures_before(pattern, close);
        output.next = close + 1u;
    } else if (value == CharT('^')) {
        output.kind = atom_bol;
        output.next = index + 1u;
    } else if (value == CharT('$')) {
        output.kind = atom_eol;
        output.next = index + 1u;
    } else {
        if (value == CharT('*') || value == CharT('+') ||
            value == CharT('?') || value == CharT('{') ||
            value == CharT('}') || value == CharT(']') ||
            value == CharT(')') ||
            value == CharT('|')) return false;
        output.kind = atom_literal;
        output.literal = value;
        output.next = index + 1u;
    }

    if (output.next < end) {
        CharT quantifier = pattern[output.next];
        size_t repeat_limit =
            output.kind == atom_group
                ? maximum_group_repeat
                : (output.kind == atom_backreference
                       ? maximum_complex_group_repeat
                       : maximum_repeat);
        if ((output.kind == atom_bol || output.kind == atom_eol ||
             output.kind == atom_word_boundary ||
             output.kind == atom_not_word_boundary ||
             output.kind == atom_positive_lookahead ||
             output.kind == atom_negative_lookahead ||
             output.kind == atom_positive_lookbehind ||
             output.kind == atom_negative_lookbehind) &&
            (quantifier == CharT('*') || quantifier == CharT('+') ||
             quantifier == CharT('?') || quantifier == CharT('{')))
            return false;
        if (quantifier == CharT('*')) {
            output.minimum = 0u;
            output.maximum = repeat_limit;
            output.quantified = true;
            ++output.next;
        } else if (quantifier == CharT('+')) {
            output.minimum = 1u;
            output.maximum = repeat_limit;
            output.quantified = true;
            ++output.next;
        } else if (quantifier == CharT('?')) {
            output.minimum = 0u;
            output.maximum = 1u;
            output.quantified = true;
            ++output.next;
        } else if (quantifier == CharT('{')) {
            size_t cursor = output.next + 1u;
            size_t minimum;
            size_t maximum;
            if (!parse_bounded_decimal(pattern, cursor, end, minimum,
                                       traits))
                return false;
            if (cursor < end && pattern[cursor] == CharT('}')) {
                maximum = minimum;
                ++cursor;
            } else if (cursor < end && pattern[cursor] == CharT(',')) {
                ++cursor;
                if (cursor < end && pattern[cursor] == CharT('}')) {
                    maximum = repeat_limit;
                    ++cursor;
                } else {
                    if (!parse_bounded_decimal(pattern, cursor, end, maximum,
                                               traits) ||
                        cursor >= end || pattern[cursor] != CharT('}'))
                        return false;
                    ++cursor;
                }
            } else {
                return false;
            }
            if (minimum > maximum || maximum > repeat_limit) return false;
            output.minimum = minimum;
            output.maximum = maximum;
            output.quantified = true;
            output.next = cursor;
        }
    }
    if (output.next < end && pattern[output.next] == CharT('?') &&
        output.quantified) {
        output.greedy = false;
        ++output.next;
    }
    return true;
}

template<typename CharT, typename Traits>
bool capture_fixed_width(const basic_string<CharT>& pattern, size_t target,
                         const Traits& traits, size_t& width) {
    if (target == 0u) return false;
    size_t cursor = 0u;
    size_t capture = 0u;
    while (cursor < pattern.size()) {
        if (pattern[cursor] == CharT('\\')) {
            cursor += cursor + 1u < pattern.size() ? 2u : 1u;
            continue;
        }
        if (pattern[cursor] == CharT('[')) {
            size_t next = 0u;
            if (!skip_character_class(pattern, cursor, pattern.size(), next))
                return false;
            cursor = next;
            continue;
        }
        if (pattern[cursor] != CharT('(')) {
            ++cursor;
            continue;
        }
        size_t close = 0u;
        if (!find_group_close(pattern, cursor, pattern.size(), close))
            return false;
        const bool capturing =
            cursor + 1u >= close || pattern[cursor + 1u] != CharT('?');
        if (capturing && ++capture == target) {
            size_t inner = cursor + 1u;
            size_t total = 0u;
            while (inner < close) {
                atom<CharT> parsed;
                if (!parse_atom(pattern, inner, close, parsed, traits) ||
                    parsed.next <= inner || parsed.minimum != parsed.maximum)
                    return false;
                size_t unit_width = 1u;
                if (parsed.kind == atom_class &&
                    class_contains_multicode(pattern, parsed.class_begin,
                                             parsed.class_end, traits)) {
                    if (parsed.class_negated ||
                        !class_single_multicode_width(
                            pattern, parsed.class_begin, parsed.class_end,
                            traits, unit_width))
                        return false;
                }
                switch (parsed.kind) {
                case atom_literal:
                case atom_dot:
                case atom_class:
                case atom_digit:
                case atom_not_digit:
                case atom_space:
                case atom_not_space:
                case atom_word:
                case atom_not_word:
                    if (unit_width > maximum_lookbehind_width ||
                        parsed.maximum >
                            (maximum_lookbehind_width - total) / unit_width)
                        return false;
                    total += parsed.maximum * unit_width;
                    break;
                default:
                    return false;
                }
                inner = parsed.next;
            }
            if (total == 0u) return false;
            width = total;
            return true;
        }
        ++cursor;
    }
    return false;
}

template<typename CharT, typename Traits>
bool fixed_lookbehind_width(const basic_string<CharT>& pattern,
                            size_t begin, size_t end, size_t depth,
                            size_t& width, const Traits& traits) {
    if (depth > maximum_group_nesting) return false;
    size_t index = begin;
    size_t current_width = 0u;
    size_t alternative_width = 0u;
    bool have_alternative = false;
    while (index < end) {
        if (pattern[index] == CharT('|')) {
            if (have_alternative && current_width != alternative_width)
                return false;
            if (!have_alternative) alternative_width = current_width;
            have_alternative = true;
            current_width = 0u;
            ++index;
            continue;
        }
        atom<CharT> parsed;
        if (!parse_atom(pattern, index, end, parsed, traits) ||
            parsed.next <= index || parsed.minimum != parsed.maximum)
            return false;
        size_t atom_width = 0u;
        switch (parsed.kind) {
        case atom_literal:
        case atom_dot:
        case atom_class:
        case atom_digit:
        case atom_not_digit:
        case atom_space:
        case atom_not_space:
        case atom_word:
        case atom_not_word:
            if (parsed.kind == atom_class &&
                class_contains_multicode(pattern, parsed.class_begin,
                                         parsed.class_end, traits)) {
                if (parsed.class_negated ||
                    !class_single_multicode_width(
                        pattern, parsed.class_begin, parsed.class_end, traits,
                        atom_width))
                    return false;
            } else {
                atom_width = 1u;
            }
            break;
        case atom_group:
            if (!fixed_lookbehind_width(
                    pattern, parsed.group_begin, parsed.group_end,
                    depth + 1u, atom_width, traits)) return false;
            break;
        case atom_bol:
        case atom_eol:
        case atom_word_boundary:
        case atom_not_word_boundary:
        case atom_positive_lookahead:
        case atom_negative_lookahead:
        case atom_positive_lookbehind:
        case atom_negative_lookbehind:
            atom_width = 0u;
            break;
        case atom_backreference:
            if (!capture_fixed_width(pattern, parsed.capture_index, traits,
                                     atom_width))
                return false;
            break;
        }
        if (atom_width != 0u &&
            parsed.maximum >
                (maximum_lookbehind_width - current_width) / atom_width)
            return false;
        current_width += atom_width * parsed.maximum;
        index = parsed.next;
    }
    if (have_alternative && current_width != alternative_width) return false;
    width = have_alternative ? alternative_width : current_width;
    return true;
}

template<typename CharT, typename Traits>
bool validate_expression(const basic_string<CharT>& pattern, size_t begin,
                         size_t end, size_t depth, size_t& atom_count,
                         size_t& capture_count, size_t& alternative_count,
                         bool& has_backreference, const Traits& traits) {
    size_t index = begin;
    while (index < end) {
        if (pattern[index] == CharT('|')) {
            if (++alternative_count > maximum_alternative_count) return false;
            ++index;
            continue;
        }
        atom<CharT> parsed;
        if (++atom_count > maximum_atom_count ||
            !parse_atom(pattern, index, end, parsed, traits) ||
            parsed.next <= index)
            return false;
        if (parsed.kind == atom_backreference) {
            if (parsed.capture_index > captures_before(pattern,
                                                        pattern.size()))
                return false;
            has_backreference = true;
        } else if (parsed.kind == atom_group ||
                   parsed.kind == atom_positive_lookahead ||
                   parsed.kind == atom_negative_lookahead ||
                   parsed.kind == atom_positive_lookbehind ||
                   parsed.kind == atom_negative_lookbehind) {
            if (depth >= maximum_group_nesting) return false;
            if (parsed.kind == atom_positive_lookbehind ||
                parsed.kind == atom_negative_lookbehind) {
                size_t width = 0u;
                if (!fixed_lookbehind_width(
                        pattern, parsed.group_begin, parsed.group_end,
                        depth + 1u, width, traits)) return false;
            }
            if (parsed.capturing && parsed.capture_index > capture_count)
                capture_count = parsed.capture_index;
            if (!validate_expression(pattern, parsed.group_begin,
                                     parsed.group_end, depth + 1u,
                                     atom_count, capture_count,
                                     alternative_count,
                                     has_backreference, traits)) return false;
        }
        index = parsed.next;
    }
    return true;
}

/* The matcher below is deliberately expressed in the ECMAScript token
 * language.  The other standard grammars are normalized into that language
 * before validation, rather than being accepted as an alternate spelling and
 * accidentally gaining ECMAScript-only operators. */
enum regex_grammar {
    grammar_ecmascript,
    grammar_basic,
    grammar_extended,
    grammar_awk,
    grammar_grep,
    grammar_egrep
};

inline bool select_grammar(regex_constants::syntax_option_type flags,
                           regex_grammar& grammar) {
    const unsigned int selected = static_cast<unsigned int>(flags) &
        (static_cast<unsigned int>(regex_constants::ECMAScript) |
         static_cast<unsigned int>(regex_constants::basic) |
         static_cast<unsigned int>(regex_constants::extended) |
         static_cast<unsigned int>(regex_constants::awk) |
         static_cast<unsigned int>(regex_constants::grep) |
         static_cast<unsigned int>(regex_constants::egrep));
    switch (selected) {
    case static_cast<unsigned int>(regex_constants::ECMAScript):
        grammar = grammar_ecmascript;
        return true;
    case static_cast<unsigned int>(regex_constants::basic):
        grammar = grammar_basic;
        return true;
    case static_cast<unsigned int>(regex_constants::extended):
        grammar = grammar_extended;
        return true;
    case static_cast<unsigned int>(regex_constants::awk):
        grammar = grammar_awk;
        return true;
    case static_cast<unsigned int>(regex_constants::grep):
        grammar = grammar_grep;
        return true;
    case static_cast<unsigned int>(regex_constants::egrep):
        grammar = grammar_egrep;
        return true;
    default:
        return false;
    }
}

template<typename CharT>
bool regex_normalized_append(basic_string<CharT>& output, CharT value) {
    if (output.size() >= 4096u) return false;
    size_t previous_size = output.size();
    output.push_back(value);
    return output.size() == previous_size + 1u;
}

template<typename CharT>
bool regex_normalized_append_literal(basic_string<CharT>& output,
                                     CharT value) {
    return regex_normalized_append(output, CharT('\\')) &&
           regex_normalized_append(output, value);
}

template<typename CharT>
bool regex_ecmascript_metacharacter(CharT value) {
    return value == CharT('.') || value == CharT('^') ||
           value == CharT('$') || value == CharT('*') ||
           value == CharT('+') || value == CharT('?') ||
           value == CharT('(') || value == CharT(')') ||
           value == CharT('[') || value == CharT(']') ||
           value == CharT('{') || value == CharT('}') ||
           value == CharT('|') || value == CharT('\\');
}

template<typename CharT>
bool regex_basic_literal_metacharacter(CharT value) {
    return value == CharT('+') || value == CharT('?') ||
           value == CharT('(') || value == CharT(')') ||
           value == CharT('{') || value == CharT('}') ||
           value == CharT('|') || value == CharT(']');
}

template<typename CharT>
bool normalize_awk_octal(const basic_string<CharT>& pattern, size_t begin,
                         size_t limit, basic_string<CharT>& output,
                         size_t& next);

template<typename CharT>
bool normalize_awk_hex(const basic_string<CharT>& pattern, size_t begin,
                       size_t limit, basic_string<CharT>& output,
                       size_t& next) {
    if (begin + 3u >= limit || pattern[begin] != CharT('\\') ||
        pattern[begin + 1u] != CharT('x'))
        return false;
    unsigned long value = 0ul;
    for (size_t offset = 0u; offset < 2u; ++offset) {
        CharT digit = pattern[begin + 2u + offset];
        unsigned long converted = 0ul;
        if (digit >= CharT('0') && digit <= CharT('9'))
            converted = static_cast<unsigned long>(digit - CharT('0'));
        else if (digit >= CharT('a') && digit <= CharT('f'))
            converted = static_cast<unsigned long>(digit - CharT('a')) + 10ul;
        else if (digit >= CharT('A') && digit <= CharT('F'))
            converted = static_cast<unsigned long>(digit - CharT('A')) + 10ul;
        else
            return false;
        value = value * 16ul + converted;
    }
    const unsigned long maximum = sizeof(CharT) == 1u
        ? 0xfful : (sizeof(CharT) == 2u ? 0xfffful : 0xfffffffful);
    if (value > maximum ||
        !regex_normalized_append(output, static_cast<CharT>(value)))
        return false;
    next = begin + 4u;
    return true;
}

template<typename CharT>
bool normalize_posix_class(const basic_string<CharT>& pattern, size_t index,
                           regex_grammar grammar, basic_string<CharT>& output,
                           size_t& next) {
    size_t class_end = 0u;
    if (!skip_character_class(pattern, index, pattern.size(), class_end))
        return false;
    for (size_t cursor = index; cursor < class_end; ++cursor) {
        CharT value = pattern[cursor];
        if (value != CharT('\\')) {
            if (!regex_normalized_append(output, value)) return false;
            continue;
        }
        if (++cursor >= class_end - 1u) return false;
        value = pattern[cursor];
        if (grammar == grammar_awk) {
            if (value == CharT('a') || value == CharT('n') ||
                value == CharT('r') || value == CharT('t') ||
                value == CharT('f') || value == CharT('v') ||
                value == CharT('b')) {
                CharT control = value == CharT('a') ? CharT('\a') :
                    (value == CharT('n') ? CharT('\n') :
                     (value == CharT('r') ? CharT('\r') :
                      (value == CharT('t') ? CharT('\t') :
                       (value == CharT('f') ? CharT('\f') :
                        (value == CharT('v') ? CharT('\v') :
                                                  CharT('\b'))))));
                if (!regex_normalized_append(output, control)) return false;
                continue;
            }
            if (value >= CharT('0') && value <= CharT('7')) {
                size_t octal_next = 0u;
                if (!normalize_awk_octal(pattern, cursor - 1u,
                                         class_end - 1u, output,
                                         octal_next))
                    return false;
                cursor = octal_next - 1u;
                continue;
            }
            if (value == CharT('x')) {
                size_t hex_next = 0u;
                if (!normalize_awk_hex(pattern, cursor - 1u,
                                       class_end - 1u, output, hex_next))
                    return false;
                cursor = hex_next - 1u;
                continue;
            }
        }
        /* POSIX grammars make a backslash followed by an ordinary character
         * an identity escape.  Do not let the ECMAScript engine reinterpret
         * \d, \w, \b, or a hex escape while inside a bracket expression. */
        if (value == CharT('\\') || value == CharT(']') ||
            value == CharT('-') || value == CharT('^') ||
            value == CharT('[')) {
            if (!regex_normalized_append_literal(output, value)) return false;
        } else if (!regex_normalized_append(output, value)) {
            return false;
        }
    }
    next = class_end;
    return true;
}

template<typename CharT>
bool normalize_basic_interval(const basic_string<CharT>& pattern,
                              size_t begin, basic_string<CharT>& output,
                              size_t& next) {
    size_t cursor = begin + 2u;
    if (!regex_normalized_append(output, CharT('{'))) return false;
    while (cursor < pattern.size()) {
        if (pattern[cursor] == CharT('\\') && cursor + 1u < pattern.size() &&
            pattern[cursor + 1u] == CharT('}')) {
            if (!regex_normalized_append(output, CharT('}'))) return false;
            next = cursor + 2u;
            return true;
        }
        if (!regex_normalized_append(output, pattern[cursor])) return false;
        ++cursor;
    }
    return false;
}

/* AWK accepts a bounded octal escape after a backslash.  Normalize the
 * escape to one code unit before the ECMAScript matcher sees it; forwarding
 * the digits as ordinary text would silently match the wrong pattern. */
template<typename CharT>
bool normalize_awk_octal(const basic_string<CharT>& pattern, size_t begin,
                         size_t limit, basic_string<CharT>& output,
                         size_t& next) {
    if (begin + 1u >= limit) return false;
    CharT first = pattern[begin + 1u];
    if (first < CharT('0') || first > CharT('7')) return false;

    unsigned long value = 0ul;
    size_t cursor = begin + 1u;
    size_t digits = 0u;
    while (cursor < limit && digits < 3u) {
        CharT digit = pattern[cursor];
        if (digit < CharT('0') || digit > CharT('7')) break;
        value = value * 8ul +
                static_cast<unsigned long>(digit - CharT('0'));
        ++cursor;
        ++digits;
    }

    const unsigned long maximum = sizeof(CharT) == 1u
        ? 0xfful : (sizeof(CharT) == 2u ? 0xfffful : 0xfffffffful);
    if (value > maximum) return false;
    if (!regex_normalized_append(output, static_cast<CharT>(value)))
        return false;
    next = cursor;
    return true;
}

template<typename CharT>
bool normalize_posix_pattern(const basic_string<CharT>& pattern,
                             regex_grammar grammar,
                             basic_string<CharT>& output) {
    const bool basic = grammar == grammar_basic || grammar == grammar_grep;
    const bool newline_alternative =
        grammar == grammar_grep || grammar == grammar_egrep;
    const bool awk = grammar == grammar_awk;
    bool previous_quantifier = false;

    output.clear();
    for (size_t index = 0u; index < pattern.size();) {
        CharT value = pattern[index];
        if (value == CharT('[')) {
            if (!normalize_posix_class(pattern, index, grammar, output, index))
                return false;
            previous_quantifier = false;
            continue;
        }
        if (value == CharT('\\')) {
            if (index + 1u >= pattern.size()) return false;
            CharT escaped = pattern[index + 1u];
            if (basic) {
                if (escaped == CharT('{')) {
                    if (!normalize_basic_interval(pattern, index, output,
                                                  index)) return false;
                    previous_quantifier = true;
                    continue;
                }
                if (escaped == CharT('}')) return false;
                if (escaped == CharT('(') || escaped == CharT(')') ||
                    escaped == CharT('|')) {
                    if (!regex_normalized_append(output, escaped)) return false;
                } else if (ascii_digit(escaped) && escaped != CharT('0')) {
                    if (!regex_normalized_append_literal(output, escaped))
                        return false;
                } else if (regex_ecmascript_metacharacter(escaped)) {
                    if (!regex_normalized_append_literal(output, escaped))
                        return false;
                } else if (!regex_normalized_append(output, escaped)) {
                    return false;
                }
            } else if (awk && escaped >= CharT('0') &&
                       escaped <= CharT('7')) {
                size_t octal_next = 0u;
                if (!normalize_awk_octal(pattern, index, pattern.size(),
                                         output, octal_next))
                    return false;
                index = octal_next;
                previous_quantifier = false;
                continue;
            } else if (awk && escaped == CharT('x')) {
                size_t hex_next = 0u;
                if (!normalize_awk_hex(pattern, index, pattern.size(), output,
                                       hex_next))
                    return false;
                index = hex_next;
                previous_quantifier = false;
                continue;
            } else if (awk && escaped == CharT('\\')) {
                if (!regex_normalized_append_literal(output, escaped))
                    return false;
            } else if (awk && (escaped == CharT('a') || escaped == CharT('n') ||
                               escaped == CharT('r') || escaped == CharT('t') ||
                               escaped == CharT('f') || escaped == CharT('v') ||
                               escaped == CharT('b'))) {
                CharT control = escaped == CharT('a') ? CharT('\a') :
                    (escaped == CharT('n') ? CharT('\n') :
                     (escaped == CharT('r') ? CharT('\r') :
                      (escaped == CharT('t') ? CharT('\t') :
                       (escaped == CharT('f') ? CharT('\f') :
                        (escaped == CharT('v') ? CharT('\v') :
                                                  CharT('\b'))))));
                if (!regex_normalized_append(output, control)) return false;
            } else if (regex_ecmascript_metacharacter(escaped)) {
                if (!regex_normalized_append_literal(output, escaped))
                    return false;
            } else if (!regex_normalized_append(output, escaped)) {
                return false;
            }
            index += 2u;
            previous_quantifier = false;
            continue;
        }
        if (newline_alternative && value == CharT('\n')) {
            if (!regex_normalized_append(output, CharT('|'))) return false;
            ++index;
            previous_quantifier = false;
            continue;
        }
        if (basic && regex_basic_literal_metacharacter(value)) {
            if (!regex_normalized_append_literal(output, value)) return false;
            ++index;
            previous_quantifier = false;
            continue;
        }
        if (!basic && value == CharT('(') && index + 1u < pattern.size() &&
            pattern[index + 1u] == CharT('?')) return false;
        if (!basic && value == CharT('?') && previous_quantifier) return false;
        if (!regex_normalized_append(output, value)) return false;
        previous_quantifier = !basic &&
            (value == CharT('*') || value == CharT('+') ||
             value == CharT('?') || value == CharT('}'));
        ++index;
    }
    return true;
}

template<typename CharT>
bool normalized_posix_structure_valid(const basic_string<CharT>& pattern) {
    bool branch_has_token[maximum_group_nesting + 1u] = {};
    size_t depth = 0u;
    bool saw_alternative = false;

    for (size_t index = 0u; index < pattern.size();) {
        CharT value = pattern[index];
        if (value == CharT('[')) {
            if (!skip_character_class(pattern, index, pattern.size(), index))
                return false;
            branch_has_token[depth] = true;
            continue;
        }
        if (value == CharT('\\')) {
            if (index + 1u >= pattern.size()) return false;
            branch_has_token[depth] = true;
            index += 2u;
            continue;
        }
        if (value == CharT('(')) {
            if (depth == maximum_group_nesting) return false;
            branch_has_token[depth] = true;
            branch_has_token[++depth] = false;
            ++index;
            continue;
        }
        if (value == CharT(')')) {
            if (depth == 0u || !branch_has_token[depth]) return false;
            --depth;
            ++index;
            continue;
        }
        if (value == CharT('|')) {
            if (!branch_has_token[depth]) return false;
            branch_has_token[depth] = false;
            saw_alternative = true;
            ++index;
            continue;
        }
        if (value != CharT('*') && value != CharT('+') &&
            value != CharT('?') && value != CharT('{') &&
            value != CharT('}'))
            branch_has_token[depth] = true;
        ++index;
    }
    return depth == 0u && (!saw_alternative || branch_has_token[0]);
}

} /* namespace __regex_detail */

/* ═══════════════════════════════════════════════════════════════
 * basic_regex
 * ═══════════════════════════════════════════════════════════════*/

template<typename CharT>
class regex_traits {
public:
    using char_type = CharT;
    using string_type = basic_string<CharT>;
    using locale_type = locale;
    using char_class_type = unsigned int;

    regex_traits() : locale_() {}

    static size_t length(const char_type* value) {
        size_t result = 0u;
        if (value) {
            while (value[result] != char_type()) ++result;
        }
        return result;
    }

    char_type translate(char_type value) const { return value; }

    char_type translate_nocase(char_type value) const {
        return use_facet<ctype<char_type>>(locale_).tolower(value);
    }

    template<typename ForwardIt>
    string_type transform(ForwardIt first, ForwardIt last) const {
        string_type source(first, last);
        return use_facet<collate<char_type>>(locale_).transform(
            source.data(), source.data() + source.size());
    }

    template<typename ForwardIt>
    string_type transform_primary(ForwardIt first, ForwardIt last) const {
        string_type source;
        while (first != last) {
            source.push_back(translate_nocase(*first));
            ++first;
        }
        return use_facet<collate<char_type>>(locale_).transform(
            source.data(), source.data() + source.size());
    }

    template<typename ForwardIt>
    string_type lookup_collatename(ForwardIt first, ForwardIt last) const {
        string_type name(first, last);
        if (name.size() == 1u) return name;
        struct named_character {
            const char* name;
            char value;
        };
        const named_character names[] = {
            {"NUL", '\0'}, {"alert", '\a'}, {"backspace", '\b'},
            {"tab", '\t'}, {"newline", '\n'},
            {"vertical-tab", '\v'}, {"form-feed", '\f'},
            {"carriage-return", '\r'}, {"space", ' '}
        };
        for (const named_character& candidate : names) {
            size_t index = 0u;
            while (candidate.name[index] != '\0' && index < name.size() &&
                   name[index] == static_cast<char_type>(
                       candidate.name[index]))
                ++index;
            if (index == name.size() && candidate.name[index] == '\0')
                return string_type(1u,
                    static_cast<char_type>(candidate.value));
        }
        return string_type();
    }

    template<typename ForwardIt>
    char_class_type lookup_classname(ForwardIt first, ForwardIt last,
                                     bool icase = false) const {
        char name[8] = {};
        size_t size = 0u;
        while (first != last && size + 1u < sizeof(name)) {
            char_type value = *first++;
            if (value >= char_type('A') && value <= char_type('Z'))
                value = static_cast<char_type>(
                    value + (char_type('a') - char_type('A')));
            if (static_cast<unsigned long>(value) > 0x7ful) return 0u;
            name[size++] = static_cast<char>(value);
        }
        if (first != last) return 0u;

        if (class_name_equal(name, size, "d") ||
            class_name_equal(name, size, "digit"))
            return class_digit;
        if (class_name_equal(name, size, "s") ||
            class_name_equal(name, size, "space"))
            return class_space;
        if (class_name_equal(name, size, "w"))
            return class_alnum | class_word;
        if (class_name_equal(name, size, "alnum")) return class_alnum;
        if (class_name_equal(name, size, "alpha")) return class_alpha;
        if (class_name_equal(name, size, "blank")) return class_blank;
        if (class_name_equal(name, size, "cntrl")) return class_cntrl;
        if (class_name_equal(name, size, "graph")) return class_graph;
        if (class_name_equal(name, size, "lower"))
            return icase ? class_alpha : class_lower;
        if (class_name_equal(name, size, "print")) return class_print;
        if (class_name_equal(name, size, "punct")) return class_punct;
        if (class_name_equal(name, size, "upper"))
            return icase ? class_alpha : class_upper;
        if (class_name_equal(name, size, "xdigit")) return class_xdigit;
        return 0u;
    }

    bool isctype(char_type value, char_class_type classification) const {
        const ctype<char_type>& facet = use_facet<ctype<char_type>>(locale_);
        unsigned int base = classification & class_base_mask;
        if (base != 0u && facet.is(
                static_cast<ctype_base::mask>(base), value))
            return true;
        return (classification & class_word) != 0u &&
               value == char_type('_');
    }

    int value(char_type character, int radix) const {
        if (radix != 8 && radix != 10 && radix != 16) return -1;
        int digit = -1;
        if (character >= char_type('0') && character <= char_type('9'))
            digit = static_cast<int>(character - char_type('0'));
        else if (character >= char_type('a') && character <= char_type('f'))
            digit = static_cast<int>(character - char_type('a')) + 10;
        else if (character >= char_type('A') && character <= char_type('F'))
            digit = static_cast<int>(character - char_type('A')) + 10;
        return digit >= 0 && digit < radix ? digit : -1;
    }

    locale_type imbue(locale_type selected) {
        using std::swap;
        swap(locale_, selected);
        return selected;
    }

    locale_type getloc() const { return locale_; }

private:
    static constexpr char_class_type class_space = ctype_base::space;
    static constexpr char_class_type class_print = ctype_base::print;
    static constexpr char_class_type class_cntrl = ctype_base::cntrl;
    static constexpr char_class_type class_upper = ctype_base::upper;
    static constexpr char_class_type class_lower = ctype_base::lower;
    static constexpr char_class_type class_alpha = ctype_base::alpha;
    static constexpr char_class_type class_digit = ctype_base::digit;
    static constexpr char_class_type class_punct = ctype_base::punct;
    static constexpr char_class_type class_xdigit = ctype_base::xdigit;
    static constexpr char_class_type class_blank = ctype_base::blank;
    static constexpr char_class_type class_alnum = ctype_base::alnum;
    static constexpr char_class_type class_graph = ctype_base::graph;
    static constexpr char_class_type class_base_mask = 0xffffu;
    static constexpr char_class_type class_word = 1u << 16u;

    locale_type locale_;

    static bool class_name_equal(const char* name, size_t size,
                                 const char* expected) {
        size_t index = 0u;
        while (index < size && expected[index] != '\0' &&
               name[index] == expected[index])
            ++index;
        return index == size && expected[index] == '\0';
    }
};

template<typename CharT, typename Traits = regex_traits<CharT>>
class basic_regex {
public:
    using value_type = CharT;
    using traits_type = Traits;
    using string_type = typename traits_type::string_type;
    using locale_type = typename traits_type::locale_type;
    using flag_type = regex_constants::syntax_option_type;

    static constexpr flag_type icase = regex_constants::icase;
    static constexpr flag_type nosubs = regex_constants::nosubs;
    static constexpr flag_type optimize = regex_constants::optimize;
    static constexpr flag_type collate = regex_constants::collate;
    static constexpr flag_type ECMAScript = regex_constants::ECMAScript;
    static constexpr flag_type basic = regex_constants::basic;
    static constexpr flag_type extended = regex_constants::extended;
    static constexpr flag_type awk = regex_constants::awk;
    static constexpr flag_type grep = regex_constants::grep;
    static constexpr flag_type egrep = regex_constants::egrep;
    static constexpr flag_type multiline = regex_constants::multiline;

    basic_regex()
        : flags_(regex_constants::ECMAScript), pattern_(), valid_(false),
          mark_count_(0u), traits_() {}

    explicit basic_regex(const CharT* pattern,
                        flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(pattern, f); }

    explicit basic_regex(const basic_string<CharT>& pattern,
                        flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(pattern.data(), pattern.size(), f); }

    template<typename StringTraits, typename Allocator>
    explicit basic_regex(
        const basic_string<CharT, StringTraits, Allocator>& pattern,
        flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(pattern.data(), pattern.size(), f); }

    basic_regex(const CharT* pattern, size_t count,
                flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(pattern, count, f); }

    template<typename ForwardIt>
    basic_regex(ForwardIt first, ForwardIt last,
                flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(first, last, f); }

    basic_regex(initializer_list<CharT> pattern,
                flag_type f = regex_constants::ECMAScript)
        : basic_regex() { assign(pattern, f); }

    basic_regex(const basic_regex&) = default;
    basic_regex(basic_regex&&) noexcept = default;
    basic_regex& operator=(const basic_regex&) = default;
    basic_regex& operator=(basic_regex&&) noexcept = default;

    basic_regex& operator=(const CharT* pattern) {
        return assign(pattern);
    }

    template<typename StringTraits, typename Allocator>
    basic_regex& operator=(
        const basic_string<CharT, StringTraits, Allocator>& pattern) {
        return assign(pattern);
    }

    basic_regex& operator=(initializer_list<CharT> pattern) {
        return assign(pattern);
    }

    basic_regex& assign(const basic_regex& other) {
        flags_ = other.flags_;
        pattern_ = other.pattern_;
        valid_ = other.valid_;
        mark_count_ = other.mark_count_;
        traits_ = other.traits_;
        return *this;
    }

    basic_regex& assign(basic_regex&& other) noexcept {
        *this = std::move(other);
        return *this;
    }

    basic_regex& assign(const CharT* pattern,
                       flag_type f = regex_constants::ECMAScript) {
        if (!pattern) return assign(nullptr, 0, f);
        size_t count = 0u;
        while (count <= maximum_pattern_size &&
               pattern[count] != CharT())
            ++count;
        return assign(pattern, count, f);
    }

    basic_regex& assign(const CharT* pattern, size_t count,
                       flag_type f = regex_constants::ECMAScript) {
        basic_string<CharT> selected;
        bool selected_valid = pattern != nullptr &&
                              count <= maximum_pattern_size;
        if (selected_valid) {
            selected.assign(pattern, count);
            selected_valid = selected.size() == count;
        }
        flags_ = f;
        pattern_.swap(selected);
        valid_ = selected_valid;
        mark_count_ = 0u;
        compile();
        return *this;
    }

    template<typename StringTraits, typename Allocator>
    basic_regex& assign(
        const basic_string<CharT, StringTraits, Allocator>& pattern,
        flag_type f = regex_constants::ECMAScript) {
        return assign(pattern.data(), pattern.size(), f);
    }

    template<typename ForwardIt>
    basic_regex& assign(ForwardIt first, ForwardIt last,
                        flag_type f = regex_constants::ECMAScript) {
        basic_string<CharT> selected;
        bool selected_valid = true;
        size_t count = 0u;
        while (first != last) {
            if (count == maximum_pattern_size) {
                selected_valid = false;
                break;
            }
            selected.push_back(static_cast<CharT>(*first));
            ++first;
            ++count;
            if (selected.size() != count) {
                selected_valid = false;
                break;
            }
        }
        flags_ = f;
        pattern_.swap(selected);
        valid_ = selected_valid;
        mark_count_ = 0u;
        compile();
        return *this;
    }

    basic_regex& assign(initializer_list<CharT> pattern,
                        flag_type f = regex_constants::ECMAScript) {
        return assign(pattern.begin(), pattern.end(), f);
    }

    unsigned mark_count() const {
        return (flags_ & regex_constants::nosubs) != 0
            ? 0u : static_cast<unsigned>(mark_count_);
    }
    flag_type flags() const { return flags_; }

    locale_type imbue(locale_type selected) {
        locale_type previous = traits_.imbue(selected);
        pattern_.clear();
        valid_ = false;
        mark_count_ = 0u;
        return previous;
    }

    locale_type getloc() const { return traits_.getloc(); }

    /* swap */
    void swap(basic_regex& other)
        noexcept(is_nothrow_swappable<traits_type>::value) {
        using std::swap;
        swap(flags_, other.flags_);
        swap(pattern_, other.pattern_);
        swap(valid_, other.valid_);
        swap(mark_count_, other.mark_count_);
        swap(traits_, other.traits_);
    }

    /* Internal accessors used by the header-only matcher. */
    const basic_string<CharT>& __pattern() const { return pattern_; }
    bool __valid() const { return valid_; }
    const traits_type& __traits() const { return traits_; }

private:
    static constexpr size_t maximum_pattern_size = 4096u;

    flag_type flags_;
    basic_string<CharT> pattern_;
    bool valid_;
    size_t mark_count_;
    traits_type traits_;

    void compile() {
        const flag_type supported = regex_constants::ECMAScript |
            regex_constants::basic | regex_constants::extended |
            regex_constants::awk | regex_constants::grep |
            regex_constants::egrep |
            regex_constants::icase | regex_constants::nosubs |
            regex_constants::optimize | regex_constants::collate |
            regex_constants::multiline;
        __regex_detail::regex_grammar grammar;
        if (pattern_.size() > maximum_pattern_size ||
            (flags_ & ~supported) != 0 ||
            !__regex_detail::select_grammar(flags_, grammar) ||
            (grammar != __regex_detail::grammar_ecmascript &&
             (flags_ & regex_constants::multiline) != 0)) {
            valid_ = false;
        }
        if (!valid_) return;
        if (grammar != __regex_detail::grammar_ecmascript) {
            basic_string<CharT> normalized;
            if (!__regex_detail::normalize_posix_pattern(
                    pattern_, grammar, normalized) ||
                !__regex_detail::normalized_posix_structure_valid(normalized)) {
                valid_ = false;
            } else {
                pattern_.swap(normalized);
            }
        }
        if (!valid_) return;
        size_t index = 0u;
        size_t end = pattern_.size();
        size_t atom_count = 0u;
        size_t alternative_count = 1u;
        bool has_backreference = false;
        valid_ = __regex_detail::validate_expression(
            pattern_, index, end, 0u, atom_count, mark_count_,
            alternative_count, has_backreference, traits_);
        if ((flags_ & regex_constants::nosubs) != 0 && has_backreference)
            valid_ = false;
        if (!valid_) mark_count_ = 0u;
    }
};

using regex = basic_regex<char>;
using wregex = basic_regex<wchar_t>;

template<typename CharT, typename Traits>
void swap(basic_regex<CharT, Traits>& left,
          basic_regex<CharT, Traits>& right)
    noexcept(noexcept(left.swap(right))) {
    left.swap(right);
}

namespace __regex_detail {

static constexpr regex_constants::match_flag_type internal_match_collate =
    static_cast<regex_constants::match_flag_type>(1u << 31u);

template<typename CharT>
CharT ascii_fold(CharT value) {
    return value >= CharT('A') && value <= CharT('Z')
        ? static_cast<CharT>(value + (CharT('a') - CharT('A'))) : value;
}

template<typename CharT>
CharT ascii_upper(CharT value) {
    return value >= CharT('a') && value <= CharT('z')
        ? static_cast<CharT>(value - (CharT('a') - CharT('A'))) : value;
}

template<typename CharT, typename Traits>
bool equal(CharT left, CharT right, const Traits& traits, bool icase,
           bool collating) {
    if (icase)
        return traits.translate_nocase(left) ==
               traits.translate_nocase(right);
    if (collating)
        return traits.translate(left) == traits.translate(right);
    return left == right;
}

template<typename CharT, typename Traits>
typename Traits::string_type range_sort_key(CharT value,
                                             const Traits& traits,
                                             bool icase) {
    CharT translated = icase ? traits.translate_nocase(value)
                             : traits.translate(value);
    typename Traits::string_type source(1u, translated);
    return traits.transform(source.begin(), source.end());
}

template<typename CharT, typename Traits>
bool range_match(CharT first, CharT last, CharT actual,
                 const Traits& traits, bool icase, bool collating) {
    if (collating) {
        typename Traits::string_type first_key =
            range_sort_key(first, traits, icase);
        typename Traits::string_type last_key =
            range_sort_key(last, traits, icase);
        typename Traits::string_type actual_key =
            range_sort_key(actual, traits, icase);
        return first_key <= actual_key && actual_key <= last_key;
    }
    if (actual >= first && actual <= last) return true;
    if (!icase) return false;
    CharT folded = ascii_fold(actual);
    CharT upper = ascii_upper(actual);
    CharT translated = traits.translate_nocase(actual);
    return (folded >= first && folded <= last) ||
           (upper >= first && upper <= last) ||
           (translated >= first && translated <= last);
}

template<typename CharT>
bool line_terminator(CharT value) {
    if (value == CharT('\n') || value == CharT('\r')) return true;
    if (sizeof(CharT) > 1) {
        return value == static_cast<CharT>(0x2028) ||
               value == static_cast<CharT>(0x2029);
    }
    return false;
}

inline bool valid_match_flags(regex_constants::match_flag_type flags) {
    const regex_constants::match_flag_type supported =
        regex_constants::match_not_bol | regex_constants::match_not_eol |
        regex_constants::match_not_bow | regex_constants::match_not_eow |
        regex_constants::match_any | regex_constants::match_not_null |
        regex_constants::match_continuous |
        regex_constants::match_prev_avail;
    return (flags & ~supported) == 0;
}

template<typename CharT, typename Traits>
bool predefined_match(atom_kind kind, CharT value, const Traits& traits) {
    CharT class_name;
    bool negated = false;
    switch (kind) {
    case atom_digit: class_name = CharT('d'); break;
    case atom_not_digit: class_name = CharT('d'); negated = true; break;
    case atom_space: class_name = CharT('s'); break;
    case atom_not_space: class_name = CharT('s'); negated = true; break;
    case atom_word: class_name = CharT('w'); break;
    case atom_not_word: class_name = CharT('w'); negated = true; break;
    default: return false;
    }
    bool matched = traits.isctype(value, traits.lookup_classname(
        &class_name, &class_name + 1u));
    return negated ? !matched : matched;
}

template<typename CharT, typename Traits>
bool class_match(const basic_string<CharT>& pattern,
                 const atom<CharT>& parsed, CharT actual,
                 const Traits& traits, bool icase, bool collating) {
    bool matched = false;
    size_t cursor = parsed.class_begin;
    while (cursor < parsed.class_end) {
        class_special special;
        if (parse_class_special(pattern, cursor, parsed.class_end, special)) {
            size_t after_first = special.next;
            if (after_first < parsed.class_end &&
                pattern[after_first] == CharT('-') &&
                after_first + 1u < parsed.class_end) {
                CharT first_literal = CharT();
                CharT last_literal = CharT();
                size_t after_last = 0u;
                if (!class_special_value(pattern, special, traits,
                                         first_literal) ||
                    !class_range_endpoint(
                        pattern, after_first + 1u, parsed.class_end,
                        traits, last_literal, after_last))
                    return false;
                if (range_match(first_literal, last_literal, actual, traits,
                                icase, collating))
                    matched = true;
                cursor = after_last;
                continue;
            }
            auto first = pattern.begin() + special.name_begin;
            auto last = pattern.begin() + special.name_end;
            if (special.kind == class_special_named) {
                if (traits.isctype(
                        actual,
                        traits.lookup_classname(first, last, icase)))
                    matched = true;
            } else {
                typename Traits::string_type collating_element =
                    traits.lookup_collatename(first, last);
                if (collating_element.size() != 1u) return false;
                if (special.kind == class_special_collating) {
                    if (equal(actual, collating_element[0], traits, icase,
                              collating))
                        matched = true;
                } else {
                    typename Traits::string_type actual_string(1u, actual);
                    if (traits.transform_primary(actual_string.begin(),
                                                 actual_string.end()) ==
                        traits.transform_primary(
                            collating_element.begin(),
                            collating_element.end()))
                        matched = true;
                }
            }
            cursor = special.next;
            continue;
        }
        atom_kind first_kind;
        CharT first_literal = CharT();
        size_t after_first;
        if (!class_unit(pattern, cursor, parsed.class_end, first_kind,
                        first_literal, after_first, traits)) return false;
        cursor = after_first;
        if (first_kind == atom_literal && cursor < parsed.class_end &&
            pattern[cursor] == CharT('-') &&
            cursor + 1u < parsed.class_end) {
            CharT last_literal = CharT();
            size_t after_last;
            if (!class_range_endpoint(
                    pattern, cursor + 1u, parsed.class_end, traits,
                    last_literal, after_last)) return false;
            if (range_match(first_literal, last_literal, actual, traits,
                            icase, collating))
                matched = true;
            cursor = after_last;
        } else if (first_kind == atom_literal) {
            if (equal(actual, first_literal, traits, icase, collating))
                matched = true;
        } else if (predefined_match(first_kind, actual, traits)) {
            matched = true;
        }
    }
    return parsed.class_negated ? !matched : matched;
}

/* Match a class and return the number of input code units it consumes.  The
 * historical class_match above remains the one-code-unit helper; this
 * bounded overload additionally handles a traits-provided multi-code-unit
 * collating/equivalence element. */
template<typename BidirIt, typename CharT, typename Traits>
bool class_match_width(const basic_string<CharT>& pattern,
                       const atom<CharT>& parsed, BidirIt cursor,
                       BidirIt range_last, const Traits& traits, bool icase,
                       bool collating, size_t& width) {
    width = 0u;
    bool matched = false;
    size_t cursor_index = parsed.class_begin;
    while (cursor_index < parsed.class_end) {
        class_special special;
        if (parse_class_special(pattern, cursor_index, parsed.class_end,
                                special)) {
            size_t after_first = special.next;
            if (after_first < parsed.class_end &&
                pattern[after_first] == CharT('-') &&
                after_first + 1u < parsed.class_end) {
                if (class_special_is_multicode(pattern, special, traits)) {
                    typename Traits::string_type first_element =
                        traits.lookup_collatename(
                            pattern.begin() + special.name_begin,
                            pattern.begin() + special.name_end);
                    typename Traits::string_type last_element;
                    size_t after_last = 0u;
                    if ((special.kind != class_special_collating &&
                         special.kind != class_special_equivalence) ||
                        !class_range_element(
                            pattern, after_first + 1u, parsed.class_end,
                            traits, last_element, after_last) ||
                        last_element.size() != first_element.size())
                        return false;
                    BidirIt probe = cursor;
                    typename Traits::string_type actual;
                    bool element_match = true;
                    for (size_t offset = 0u; offset < first_element.size();
                         ++offset) {
                        if (probe == range_last) {
                            element_match = false;
                            break;
                        }
                        actual.push_back(static_cast<CharT>(*probe));
                        ++probe;
                    }
                    if (element_match) {
                        typename Traits::string_type first_key =
                            special.kind == class_special_collating
                                ? traits.transform(first_element.begin(),
                                                   first_element.end())
                                : traits.transform_primary(
                                      first_element.begin(),
                                      first_element.end());
                        typename Traits::string_type last_key =
                            special.kind == class_special_collating
                                ? traits.transform(last_element.begin(),
                                                   last_element.end())
                                : traits.transform_primary(
                                      last_element.begin(),
                                      last_element.end());
                        typename Traits::string_type actual_key =
                            special.kind == class_special_collating
                                ? traits.transform(actual.begin(), actual.end())
                                : traits.transform_primary(actual.begin(),
                                                            actual.end());
                        if (first_key <= actual_key && actual_key <= last_key) {
                            matched = true;
                            if (width < first_element.size())
                                width = first_element.size();
                        }
                    }
                    cursor_index = after_last;
                    continue;
                }
                CharT first_literal = CharT();
                CharT last_literal = CharT();
                size_t after_last = 0u;
                if (!class_special_value(pattern, special, traits,
                                         first_literal) ||
                    !class_range_endpoint(pattern, after_first + 1u,
                                          parsed.class_end, traits,
                                          last_literal, after_last))
                    return false;
                if (cursor != range_last &&
                    range_match(first_literal, last_literal,
                                static_cast<CharT>(*cursor), traits, icase,
                                collating)) {
                    matched = true;
                    if (width < 1u) width = 1u;
                }
                cursor_index = after_last;
                continue;
            }
            auto first = pattern.begin() + special.name_begin;
            auto last = pattern.begin() + special.name_end;
            if (special.kind == class_special_named) {
                if (cursor != range_last && traits.isctype(
                        static_cast<CharT>(*cursor),
                        traits.lookup_classname(first, last, icase))) {
                    matched = true;
                    if (width < 1u) width = 1u;
                }
            } else {
                typename Traits::string_type element =
                    traits.lookup_collatename(first, last);
                if (element.empty() ||
                    element.size() > maximum_collating_element_width)
                    return false;
                BidirIt probe = cursor;
                typename Traits::string_type actual;
                bool element_match = true;
                for (size_t offset = 0u; offset < element.size();
                     ++offset) {
                    if (probe == range_last) {
                        element_match = false;
                        break;
                    }
                    actual.push_back(static_cast<CharT>(*probe));
                    ++probe;
                }
                if (element_match && special.kind == class_special_collating) {
                    probe = cursor;
                    for (size_t offset = 0u; offset < element.size();
                         ++offset, ++probe) {
                        if (!equal(static_cast<CharT>(*probe), element[offset],
                                   traits, icase, collating)) {
                            element_match = false;
                            break;
                        }
                    }
                } else if (element_match) {
                    element_match =
                        traits.transform_primary(actual.begin(), actual.end()) ==
                        traits.transform_primary(element.begin(), element.end());
                }
                if (element_match) {
                    matched = true;
                    if (width < element.size()) width = element.size();
                }
            }
            cursor_index = special.next;
            continue;
        }
        atom_kind first_kind;
        CharT first_literal = CharT();
        size_t after_first = 0u;
        if (!class_unit(pattern, cursor_index, parsed.class_end, first_kind,
                       first_literal, after_first, traits))
            return false;
        cursor_index = after_first;
        if (first_kind == atom_literal && cursor_index < parsed.class_end &&
            pattern[cursor_index] == CharT('-') &&
            cursor_index + 1u < parsed.class_end) {
            CharT last_literal = CharT();
            size_t after_last = 0u;
            if (!class_range_endpoint(pattern, cursor_index + 1u,
                                      parsed.class_end, traits, last_literal,
                                      after_last))
                return false;
            if (cursor != range_last &&
                range_match(first_literal, last_literal,
                            static_cast<CharT>(*cursor), traits, icase,
                            collating)) {
                matched = true;
                if (width < 1u) width = 1u;
            }
            cursor_index = after_last;
        } else if (first_kind == atom_literal) {
            if (cursor != range_last &&
                equal(static_cast<CharT>(*cursor), first_literal, traits,
                      icase, collating)) {
                matched = true;
                if (width < 1u) width = 1u;
            }
        } else if (cursor != range_last &&
                   predefined_match(first_kind,
                                    static_cast<CharT>(*cursor), traits)) {
            matched = true;
            if (width < 1u) width = 1u;
        }
    }
    if (parsed.class_negated && width > 1u) return false;
    if (parsed.class_negated) {
        if (cursor == range_last) return false;
        width = 1u;
        return !matched;
    }
    return matched && width != 0u;
}

template<typename CharT, typename Traits>
bool atom_match(const basic_string<CharT>& pattern,
                const atom<CharT>& parsed, CharT actual,
                const Traits& traits, bool icase, bool collating) {
    if (parsed.kind == atom_literal)
        return equal(actual, parsed.literal, traits, icase, collating);
    if (parsed.kind == atom_dot) return !line_terminator(actual);
    if (parsed.kind == atom_class)
        return class_match(pattern, parsed, actual, traits, icase,
                           collating);
    return predefined_match(parsed.kind, actual, traits);
}

template<typename BidirIt, typename CharT, typename Traits>
bool atom_match_width(const basic_string<CharT>& pattern,
                      const atom<CharT>& parsed, BidirIt cursor,
                      BidirIt range_last, const Traits& traits, bool icase,
                      bool collating, size_t& width) {
    width = 1u;
    if (parsed.kind == atom_class)
        return class_match_width(pattern, parsed, cursor, range_last, traits,
                                 icase, collating, width);
    if (cursor == range_last) return false;
    return atom_match(pattern, parsed, static_cast<CharT>(*cursor), traits,
                      icase, collating);
}

template<typename BidirIt>
void advance_code_units(BidirIt& cursor, size_t width) {
    for (size_t index = 0u; index < width; ++index) ++cursor;
}

template<typename BidirIt>
struct capture_record {
    BidirIt first{};
    BidirIt second{};
    bool matched = false;
};

template<typename BidirIt>
struct capture_set {
    capture_record<BidirIt>* values;

    capture_set() noexcept : values(nullptr), valid_(false) {
        allocate();
    }

    capture_set(const capture_set& other) noexcept
        : values(nullptr), valid_(false) {
        allocate();
        if (!valid_) return;
        if (!other.valid_) {
            clear();
            valid_ = false;
            return;
        }
        for (size_t index = 0u; index < maximum_capture_count; ++index)
            values[index] = other.values[index];
    }

    capture_set(capture_set&& other) noexcept
        : values(other.values), valid_(other.valid_) {
        other.values = nullptr;
        other.valid_ = false;
    }

    capture_set& operator=(const capture_set& other) noexcept {
        if (this == &other) return *this;
        if (!valid_) allocate();
        if (!valid_) return *this;
        if (!other.valid_) {
            clear();
            valid_ = false;
            return *this;
        }
        for (size_t index = 0u; index < maximum_capture_count; ++index)
            values[index] = other.values[index];
        return *this;
    }

    capture_set& operator=(capture_set&& other) noexcept {
        if (this == &other) return *this;
        release();
        values = other.values;
        valid_ = other.valid_;
        other.values = nullptr;
        other.valid_ = false;
        return *this;
    }

    ~capture_set() { release(); }

    bool valid() const noexcept { return valid_; }

private:
    bool valid_;

    void allocate() noexcept {
        values = static_cast<capture_record<BidirIt>*>(
            rin_malloc(sizeof(capture_record<BidirIt>) *
                       maximum_capture_count));
        if (!values) return;
        for (size_t index = 0u; index < maximum_capture_count; ++index)
            ::new (static_cast<void*>(&values[index])) capture_record<BidirIt>();
        valid_ = true;
    }

    void clear() noexcept {
        if (!values) return;
        for (size_t index = 0u; index < maximum_capture_count; ++index)
            values[index] = capture_record<BidirIt>();
    }

    void release() noexcept {
        if (!values) return;
        for (size_t index = 0u; index < maximum_capture_count; ++index)
            values[index].~capture_record<BidirIt>();
        rin_free(values);
        values = nullptr;
        valid_ = false;
    }
};

template<typename BidirIt>
struct continuation_frame {
    size_t index = 0u;
    size_t end = 0u;
    size_t capture_index = 0u;
    size_t capture_begin = 1u;
    size_t capture_end = 0u;
    size_t group_begin = 0u;
    size_t group_end = 0u;
    size_t minimum = 1u;
    size_t maximum = 1u;
    size_t count = 0u;
    size_t depth = 0u;
    bool greedy = true;
    BidirIt iteration_start{};
};

template<typename BidirIt>
void reset_captures(capture_set<BidirIt>& captures, size_t begin,
                    size_t end) {
    if (begin == 0u || begin > end) return;
    for (size_t index = begin; index <= end; ++index)
        captures.values[index - 1u] = capture_record<BidirIt>();
}

template<typename CharT>
size_t alternative_end(const basic_string<CharT>& pattern, size_t begin,
                       size_t end) {
    size_t depth = 0u;
    size_t cursor = begin;
    while (cursor < end) {
        if (pattern[cursor] == CharT('\\')) {
            cursor += 2u;
        } else if (pattern[cursor] == CharT('[')) {
            size_t next;
            if (!skip_character_class(pattern, cursor, end, next)) return end;
            cursor = next;
        } else if (pattern[cursor] == CharT('(')) {
            ++depth;
            ++cursor;
        } else if (pattern[cursor] == CharT(')')) {
            if (depth != 0u) --depth;
            ++cursor;
        } else if (pattern[cursor] == CharT('|') && depth == 0u) {
            return cursor;
        } else {
            ++cursor;
        }
    }
    return end;
}

template<typename BidirIt, typename CharT>
bool terminal_match(BidirIt range_last, BidirIt candidate, BidirIt cursor,
                    bool require_full,
                    regex_constants::match_flag_type flags,
                    BidirIt& match_end) {
    if (require_full && cursor != range_last) return false;
    if ((flags & regex_constants::match_not_null) != 0 &&
        cursor == candidate) return false;
    match_end = cursor;
    return true;
}

template<typename BidirIt, typename CharT>
bool beginning_assertion(BidirIt range_first, BidirIt cursor, bool multiline,
                         regex_constants::match_flag_type flags) {
    if (cursor == range_first) {
        if ((flags & regex_constants::match_prev_avail) != 0) {
            BidirIt previous = cursor;
            --previous;
            return multiline &&
                   line_terminator(static_cast<CharT>(*previous));
        }
        return (flags & regex_constants::match_not_bol) == 0;
    }
    BidirIt previous = cursor;
    --previous;
    return multiline && line_terminator(static_cast<CharT>(*previous));
}

template<typename BidirIt, typename CharT>
bool ending_assertion(BidirIt range_last, BidirIt cursor, bool multiline,
                      regex_constants::match_flag_type flags) {
    if (cursor == range_last)
        return (flags & regex_constants::match_not_eol) == 0;
    return multiline &&
           line_terminator(static_cast<CharT>(*cursor));
}

template<typename BidirIt, typename CharT, typename Traits>
bool word_boundary_assertion(BidirIt range_first, BidirIt range_last,
                             BidirIt cursor,
                             regex_constants::match_flag_type flags,
                             const Traits& traits) {
    bool left_word = false;
    bool right_word = false;
    bool have_previous = cursor != range_first ||
                         (flags & regex_constants::match_prev_avail) != 0;
    if (have_previous) {
        BidirIt previous = cursor;
        --previous;
        left_word = predefined_match(
            atom_word, static_cast<CharT>(*previous), traits);
    }
    if (cursor != range_last)
        right_word = predefined_match(
            atom_word, static_cast<CharT>(*cursor), traits);

    bool boundary = left_word != right_word;
    if (boundary && cursor == range_first && !have_previous && right_word &&
        (flags & regex_constants::match_not_bow) != 0)
        boundary = false;
    if (boundary && cursor == range_last && left_word &&
        (flags & regex_constants::match_not_eow) != 0)
        boundary = false;
    return boundary;
}

template<typename BidirIt, typename CharT, typename Traits>
bool match_sequence(const basic_string<CharT>& pattern, const Traits& traits,
                    size_t index,
                    size_t sequence_end, BidirIt range_first,
                    BidirIt range_last, BidirIt candidate, BidirIt cursor,
                    bool require_full, bool multiline, bool icase,
                    regex_constants::match_flag_type flags, size_t depth,
                    size_t& operations, capture_set<BidirIt>& captures,
                    continuation_frame<BidirIt>* frames, size_t frame_count,
                    BidirIt& match_end);

template<typename BidirIt, typename CharT, typename Traits>
bool match_disjunction(const basic_string<CharT>& pattern,
                       const Traits& traits, size_t begin,
                       size_t end, BidirIt range_first, BidirIt range_last,
                       BidirIt candidate, BidirIt cursor, bool require_full,
                       bool multiline, bool icase,
                       regex_constants::match_flag_type flags, size_t depth,
                       size_t& operations, capture_set<BidirIt>& captures,
                       continuation_frame<BidirIt>* frames,
                       size_t frame_count, BidirIt& match_end) {
    size_t alternative = begin;
    for (;;) {
        size_t finish = alternative_end(pattern, alternative, end);
        if (operations == 0u) return false;
        --operations;
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, alternative, finish, range_first,
                           range_last, candidate, cursor, require_full,
                           multiline, icase, flags, depth, operations,
                           captures, frames, frame_count, match_end))
            return true;
        captures = before;
        if (finish == end) return false;
        alternative = finish + 1u;
    }
}

template<typename BidirIt, typename CharT, typename Traits>
bool single_unit_group(const basic_string<CharT>& pattern,
                       const Traits& traits,
                       const continuation_frame<BidirIt>& state,
                       atom<CharT>& inner) {
    if (!parse_atom(pattern, state.group_begin, state.group_end, inner,
                    traits) ||
        inner.next != state.group_end || inner.quantified ||
        inner.minimum != 1u || inner.maximum != 1u)
        return false;
    if (inner.kind == atom_class &&
        class_contains_multicode(pattern, inner.class_begin, inner.class_end,
                                 traits))
        return false;
    switch (inner.kind) {
    case atom_literal:
    case atom_dot:
    case atom_class:
    case atom_digit:
    case atom_not_digit:
    case atom_space:
    case atom_not_space:
    case atom_word:
    case atom_not_word:
        return true;
    default:
        return false;
    }
}

/* Match a deterministic one-code-unit group without recursively entering
 * the group for every repetition.  This keeps large intervals such as
 * `(a){4097}` bounded by the shared operation budget rather than the host
 * thread stack.  More expressive groups use match_group_repeat below so
 * their alternative/capture rollback semantics stay unchanged. */
template<typename BidirIt, typename CharT, typename Traits>
bool match_single_unit_group_repeat(
    const basic_string<CharT>& pattern, const Traits& traits,
    const continuation_frame<BidirIt>& state, BidirIt range_first,
    BidirIt range_last, BidirIt candidate, BidirIt cursor, bool require_full,
    bool multiline, bool icase, regex_constants::match_flag_type flags,
    size_t& operations, capture_set<BidirIt>& captures,
    continuation_frame<BidirIt>* frames, size_t frame_count,
    BidirIt& match_end, const atom<CharT>& inner) {
    const bool collating = (flags & internal_match_collate) != 0;
    BidirIt current = cursor;
    size_t count = 0u;

    /* A non-greedy group first tries the continuation with zero iterations. */
    if (!state.greedy && state.minimum == 0u) {
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, state.index, state.end,
                           range_first, range_last, candidate, current,
                           require_full, multiline, icase, flags, state.depth,
                           operations, captures, frames, frame_count,
                           match_end))
            return true;
        captures = before;
    }

    while (count < state.maximum && current != range_last) {
        if (operations == 0u) return false;
        --operations;
        if (!atom_match(pattern, inner, static_cast<CharT>(*current), traits,
                        icase, collating))
            break;
        BidirIt next = current;
        ++next;
        if (state.capture_index != 0u) {
            capture_record<BidirIt>& captured =
                captures.values[state.capture_index - 1u];
            captured.first = current;
            captured.second = next;
            captured.matched = true;
        }
        current = next;
        ++count;

        if (!state.greedy && count >= state.minimum) {
            capture_set<BidirIt> before = captures;
            if (!before.valid()) return false;
            if (match_sequence(pattern, traits, state.index, state.end,
                               range_first, range_last, candidate, current,
                               require_full, multiline, icase, flags,
                               state.depth, operations, captures, frames,
                               frame_count, match_end))
                return true;
            captures = before;
        }
    }

    if (count < state.minimum) return false;
    if (!state.greedy) return false;

    /* Greedy continuation backtracks one deterministic unit at a time. */
    for (;;) {
        if (operations == 0u) return false;
        --operations;
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, state.index, state.end,
                           range_first, range_last, candidate, current,
                           require_full, multiline, icase, flags, state.depth,
                           operations, captures, frames, frame_count,
                           match_end))
            return true;
        captures = before;
        if (count == state.minimum) return false;
        --current;
        --count;
        if (state.capture_index != 0u) {
            BidirIt next = current;
            ++next;
            capture_record<BidirIt>& captured =
                captures.values[state.capture_index - 1u];
            captured.first = current;
            captured.second = next;
            captured.matched = true;
        }
    }
}

template<typename BidirIt, typename CharT, typename Traits>
bool match_group_repeat(const basic_string<CharT>& pattern,
                        const Traits& traits,
                        const continuation_frame<BidirIt>& state,
                        bool can_repeat, BidirIt range_first,
                        BidirIt range_last, BidirIt candidate, BidirIt cursor,
                        bool require_full, bool multiline, bool icase,
                        regex_constants::match_flag_type flags,
                        size_t& operations, capture_set<BidirIt>& captures,
                        continuation_frame<BidirIt>* frames,
                        size_t frame_count, BidirIt& match_end) {
    atom<CharT> single;
    if (single_unit_group(pattern, traits, state, single))
        return match_single_unit_group_repeat(
            pattern, traits, state, range_first, range_last, candidate,
            cursor, require_full, multiline, icase, flags, operations,
            captures, frames, frame_count, match_end, single);

    /* Complex groups retain the original recursive continuation bound.  The
     * iterative fast path above is the only path widened to the decimal
     * interval range; clamp the fallback frame before it can consume native
     * stack for a larger-looking interval. */
    if (state.maximum > maximum_complex_group_repeat) {
        continuation_frame<BidirIt> bounded = state;
        bounded.maximum = maximum_complex_group_repeat;
        return match_group_repeat(
            pattern, traits, bounded, can_repeat, range_first, range_last,
            candidate, cursor, require_full, multiline, icase, flags,
            operations, captures, frames, frame_count, match_end);
    }

    if (state.maximum > maximum_recursive_group_repeat) {
        continuation_frame<BidirIt> bounded = state;
        bounded.maximum = maximum_recursive_group_repeat;
        return match_group_repeat(
            pattern, traits, bounded, can_repeat, range_first, range_last,
            candidate, cursor, require_full, multiline, icase, flags,
            operations, captures, frames, frame_count, match_end);
    }

    if (!state.greedy && state.count >= state.minimum) {
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, state.index, state.end, range_first,
                           range_last, candidate, cursor, require_full,
                           multiline, icase, flags, state.depth, operations,
                           captures, frames, frame_count, match_end))
            return true;
        captures = before;
    }

    if (can_repeat && state.count < state.maximum) {
        if (frame_count >= maximum_group_nesting) return false;
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        reset_captures(captures, state.capture_begin, state.capture_end);
        if (state.capture_index != 0u) {
            capture_record<BidirIt>& captured =
                captures.values[state.capture_index - 1u];
            captured.first = cursor;
            captured.second = cursor;
        }
        frames[frame_count] = state;
        frames[frame_count].count = state.count + 1u;
        frames[frame_count].iteration_start = cursor;
        if (match_disjunction(pattern, traits, state.group_begin,
                              state.group_end,
                              range_first, range_last, candidate, cursor,
                              require_full, multiline, icase, flags,
                              state.depth, operations, captures, frames,
                              frame_count + 1u, match_end)) return true;
        captures = before;
    }

    if (state.greedy && state.count >= state.minimum) {
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, state.index, state.end, range_first,
                           range_last, candidate, cursor, require_full,
                           multiline, icase, flags, state.depth, operations,
                           captures, frames, frame_count, match_end))
            return true;
        captures = before;
    }
    return false;
}

template<typename BidirIt, typename CharT, typename Traits>
bool resume_sequence(const basic_string<CharT>& pattern, const Traits& traits,
                     BidirIt range_first, BidirIt range_last,
                     BidirIt candidate, BidirIt cursor, bool require_full,
                     bool multiline, bool icase,
                     regex_constants::match_flag_type flags,
                     size_t& operations, capture_set<BidirIt>& captures,
                     continuation_frame<BidirIt>* frames,
                     size_t frame_count, BidirIt& match_end) {
    if (frame_count == 0u) {
        return terminal_match<BidirIt, CharT>(
            range_last, candidate, cursor, require_full, flags, match_end);
    }
    const continuation_frame<BidirIt> frame = frames[frame_count - 1u];
    capture_set<BidirIt> before = captures;
    if (!before.valid()) return false;
    if (frame.capture_index != 0u) {
        capture_record<BidirIt>& captured =
            captures.values[frame.capture_index - 1u];
        captured.second = cursor;
        captured.matched = true;
    }
    bool can_repeat = cursor != frame.iteration_start ||
                      frame.count < frame.minimum;
    if (match_group_repeat(pattern, traits, frame, can_repeat, range_first,
                           range_last, candidate, cursor, require_full,
                           multiline, icase, flags, operations, captures,
                           frames, frame_count - 1u, match_end)) return true;
    captures = before;
    return false;
}

template<typename BidirIt, typename CharT, typename Traits>
bool consume_backreference(const capture_record<BidirIt>& captured,
                           BidirIt range_last, BidirIt& cursor,
                           const Traits& traits, bool icase, bool collating,
                           size_t& operations) {
    BidirIt reference = captured.first;
    while (reference != captured.second) {
        if (operations == 0u || cursor == range_last) return false;
        --operations;
        if (!equal(static_cast<CharT>(*reference),
                   static_cast<CharT>(*cursor), traits, icase,
                   collating)) return false;
        ++reference;
        ++cursor;
    }
    return true;
}

template<typename BidirIt>
void rewind_backreference(const capture_record<BidirIt>& captured,
                          BidirIt& cursor) {
    BidirIt reference = captured.first;
    while (reference != captured.second) {
        --cursor;
        ++reference;
    }
}

template<typename BidirIt, typename CharT, typename Traits>
bool restore_atom_repetition(
    const basic_string<CharT>& pattern, const atom<CharT>& parsed,
    BidirIt repeat_start, size_t count, BidirIt range_last,
    const Traits& traits, bool icase, bool collating, size_t& operations,
    BidirIt& cursor) {
    cursor = repeat_start;
    for (size_t index = 0u; index < count; ++index) {
        if (operations == 0u) return false;
        --operations;
        size_t consumed = 0u;
        if (!atom_match_width(pattern, parsed, cursor, range_last, traits,
                              icase, collating, consumed))
            return false;
        advance_code_units(cursor, consumed);
    }
    return true;
}

template<typename BidirIt, typename CharT, typename Traits>
bool match_sequence(const basic_string<CharT>& pattern, const Traits& traits,
                    size_t index,
                    size_t sequence_end, BidirIt range_first,
                    BidirIt range_last, BidirIt candidate, BidirIt cursor,
                    bool require_full, bool multiline, bool icase,
                    regex_constants::match_flag_type flags, size_t depth,
                    size_t& operations, capture_set<BidirIt>& captures,
                    continuation_frame<BidirIt>* frames, size_t frame_count,
                    BidirIt& match_end) {
    if (depth > maximum_atom_count) return false;
    const bool collating =
        (flags & internal_match_collate) != 0;
    if (index == sequence_end)
        return resume_sequence<BidirIt, CharT>(
            pattern, traits, range_first, range_last, candidate, cursor,
            require_full, multiline, icase, flags, operations, captures,
            frames, frame_count, match_end);
    atom<CharT> parsed;
    if (!parse_atom(pattern, index, sequence_end, parsed, traits)) return false;

    if (parsed.kind == atom_bol || parsed.kind == atom_eol ||
        parsed.kind == atom_word_boundary ||
        parsed.kind == atom_not_word_boundary) {
        if (operations == 0u) return false;
        --operations;
        bool matched;
        if (parsed.kind == atom_bol) {
            matched = beginning_assertion<BidirIt, CharT>(
                range_first, cursor, multiline, flags);
        } else if (parsed.kind == atom_eol) {
            matched = ending_assertion<BidirIt, CharT>(
                range_last, cursor, multiline, flags);
        } else {
            matched = word_boundary_assertion<BidirIt, CharT>(
                range_first, range_last, cursor, flags, traits);
            if (parsed.kind == atom_not_word_boundary) matched = !matched;
        }
        if (!matched) return false;
        return match_sequence(pattern, traits, parsed.next, sequence_end,
                              range_first, range_last, candidate, cursor,
                              require_full, multiline, icase, flags,
                              depth + 1u, operations, captures, frames,
                              frame_count, match_end);
    }

    if (parsed.kind == atom_positive_lookahead ||
        parsed.kind == atom_negative_lookahead ||
        parsed.kind == atom_positive_lookbehind ||
        parsed.kind == atom_negative_lookbehind) {
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        continuation_frame<BidirIt> assertion_frames[maximum_group_nesting];
        bool lookbehind = parsed.kind == atom_positive_lookbehind ||
                          parsed.kind == atom_negative_lookbehind;
        bool negative = parsed.kind == atom_negative_lookahead ||
                        parsed.kind == atom_negative_lookbehind;
        BidirIt assertion_start = cursor;
        BidirIt assertion_end = cursor;
        bool have_lookbehind_input = true;
        if (lookbehind) {
            size_t width = 0u;
            if (!fixed_lookbehind_width(
                    pattern, parsed.group_begin, parsed.group_end,
                    0u, width, traits)) return false;
            for (size_t offset = 0u; offset < width; ++offset) {
                if (operations == 0u) return false;
                --operations;
                if (assertion_start == range_first) {
                    have_lookbehind_input = false;
                    break;
                }
                --assertion_start;
            }
            assertion_end = assertion_start;
        }
        reset_captures(captures, parsed.capture_begin, parsed.capture_end);
        regex_constants::match_flag_type assertion_flags =
            flags & ~regex_constants::match_not_null;
        bool assertion_matched = have_lookbehind_input && match_disjunction(
            pattern, traits, parsed.group_begin, parsed.group_end, range_first,
            range_last, assertion_start, assertion_start, false, multiline,
            icase, assertion_flags, depth + 1u, operations, captures,
            assertion_frames, 0u, assertion_end);
        if (lookbehind && assertion_matched && assertion_end != cursor)
            assertion_matched = false;
        if (negative) {
            captures = before;
            if (assertion_matched || operations == 0u) return false;
        } else if (!assertion_matched) {
            captures = before;
            return false;
        }
        if (match_sequence(pattern, traits, parsed.next, sequence_end,
                           range_first, range_last, candidate, cursor,
                           require_full, multiline, icase, flags, depth + 1u,
                           operations, captures, frames, frame_count,
                           match_end))
            return true;
        captures = before;
        return false;
    }

    if (parsed.kind == atom_group) {
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        reset_captures(captures, parsed.capture_begin, parsed.capture_end);
        continuation_frame<BidirIt> state;
        state.index = parsed.next;
        state.end = sequence_end;
        state.capture_index = parsed.capturing ? parsed.capture_index : 0u;
        state.capture_begin = parsed.capture_begin;
        state.capture_end = parsed.capture_end;
        state.group_begin = parsed.group_begin;
        state.group_end = parsed.group_end;
        state.minimum = parsed.minimum;
        state.maximum = parsed.maximum;
        state.greedy = parsed.greedy;
        state.depth = depth + 1u;
        state.iteration_start = cursor;
        if (match_group_repeat(pattern, traits, state, true, range_first,
                               range_last, candidate, cursor, require_full,
                               multiline, icase, flags, operations, captures,
                               frames, frame_count, match_end)) return true;
        captures = before;
        return false;
    }

    if (parsed.kind == atom_backreference) {
        const capture_record<BidirIt>& captured =
            captures.values[parsed.capture_index - 1u];
        if (!captured.matched || captured.first == captured.second) {
            return match_sequence(
                pattern, traits, parsed.next, sequence_end, range_first,
                range_last,
                candidate, cursor, require_full, multiline, icase, flags,
                depth + 1u, operations, captures, frames, frame_count,
                match_end);
        }
        size_t count = 0u;
        while (count < parsed.minimum) {
            if (!consume_backreference<BidirIt, CharT>(
                    captured, range_last, cursor, traits, icase, collating,
                    operations))
                return false;
            ++count;
        }
        if (parsed.greedy) {
            while (count < parsed.maximum) {
                BidirIt before_cursor = cursor;
                if (!consume_backreference<BidirIt, CharT>(
                        captured, range_last, cursor, traits, icase,
                        collating,
                        operations)) {
                    cursor = before_cursor;
                    if (operations == 0u) return false;
                    break;
                }
                ++count;
            }
            for (;;) {
                if (operations == 0u) return false;
                --operations;
                capture_set<BidirIt> before = captures;
                if (!before.valid()) return false;
                if (match_sequence(
                        pattern, traits, parsed.next, sequence_end, range_first,
                        range_last, candidate, cursor, require_full,
                        multiline, icase, flags, depth + 1u,
                        operations, captures, frames, frame_count,
                        match_end)) return true;
                captures = before;
                if (count == parsed.minimum) return false;
                rewind_backreference(captured, cursor);
                --count;
            }
        }
        for (;;) {
            if (operations == 0u) return false;
            --operations;
            capture_set<BidirIt> before = captures;
            if (!before.valid()) return false;
            if (match_sequence(pattern, traits, parsed.next, sequence_end,
                               range_first, range_last, candidate, cursor,
                               require_full, multiline, icase, flags,
                               depth + 1u, operations, captures, frames,
                               frame_count, match_end))
                return true;
            captures = before;
            if (count == parsed.maximum) return false;
            BidirIt before_cursor = cursor;
            if (!consume_backreference<BidirIt, CharT>(
                    captured, range_last, cursor, traits, icase, collating,
                    operations)) {
                cursor = before_cursor;
                return false;
            }
            ++count;
        }
    }

    size_t count = 0u;
    BidirIt repeat_start = cursor;
    bool multi_width = false;
    while (count < parsed.minimum) {
        if (operations == 0u || cursor == range_last) return false;
        --operations;
        size_t consumed = 0u;
        if (!atom_match_width(pattern, parsed, cursor, range_last, traits,
                              icase, collating, consumed))
            return false;
        if (consumed != 1u) multi_width = true;
        advance_code_units(cursor, consumed);
        ++count;
    }

    if (parsed.greedy) {
        while (count < parsed.maximum && cursor != range_last) {
            if (operations == 0u) return false;
            --operations;
            size_t consumed = 0u;
            if (!atom_match_width(pattern, parsed, cursor, range_last, traits,
                                  icase, collating, consumed)) break;
            if (consumed != 1u) multi_width = true;
            advance_code_units(cursor, consumed);
            ++count;
        }
        for (;;) {
            if (operations == 0u) return false;
            --operations;
            capture_set<BidirIt> before = captures;
            if (!before.valid()) return false;
            if (match_sequence(pattern, traits, parsed.next, sequence_end,
                               range_first, range_last, candidate, cursor,
                               require_full, multiline, icase, flags,
                               depth + 1u, operations, captures, frames,
                               frame_count, match_end))
                return true;
            captures = before;
            if (count == parsed.minimum) break;
            --count;
            if (multi_width) {
                if (!restore_atom_repetition(
                        pattern, parsed, repeat_start, count, range_last,
                        traits, icase, collating, operations, cursor))
                    return false;
            } else {
                --cursor;
            }
        }
        return false;
    }

    for (;;) {
        if (operations == 0u) return false;
        --operations;
        capture_set<BidirIt> before = captures;
        if (!before.valid()) return false;
        if (match_sequence(pattern, traits, parsed.next, sequence_end,
                           range_first, range_last, candidate, cursor,
                           require_full, multiline, icase, flags, depth + 1u,
                           operations, captures, frames, frame_count,
                           match_end))
            return true;
        captures = before;
        if (count == parsed.maximum || cursor == range_last) break;
        if (operations == 0u) return false;
        --operations;
        size_t consumed = 0u;
        if (!atom_match_width(pattern, parsed, cursor, range_last, traits,
                              icase, collating, consumed))
            break;
        if (consumed != 1u) multi_width = true;
        advance_code_units(cursor, consumed);
        ++count;
    }
    return false;
}

template<typename BidirIt, typename CharT, typename Traits>
bool match_at(BidirIt range_first, BidirIt range_last, BidirIt candidate,
              const basic_regex<CharT, Traits>& expression,
              regex_constants::match_flag_type flags,
              BidirIt& match_end, capture_set<BidirIt>& captures,
              bool require_full = false,
              bool* complexity_exceeded = nullptr) {
    const basic_string<CharT>& pattern = expression.__pattern();
    const Traits& traits = expression.__traits();
    const bool multiline =
        (expression.flags() & regex_constants::multiline) != 0;
    const bool icase = (expression.flags() & regex_constants::icase) != 0;
    if ((expression.flags() & regex_constants::collate) != 0)
        flags |= internal_match_collate;
    size_t index = 0u;
    size_t pattern_end = pattern.size();
    size_t operations = maximum_match_operations;
    continuation_frame<BidirIt> frames[maximum_group_nesting];
    if (!captures.valid()) return false;
    reset_captures(captures, 1u, maximum_capture_count);

    const bool matched = match_disjunction(
        pattern, traits, index, pattern_end, range_first, range_last,
        candidate, candidate, require_full, multiline, icase, flags, 0u,
        operations, captures, frames, 0u, match_end);
    if (complexity_exceeded) {
        *complexity_exceeded = !matched && operations == 0u;
    }
    return matched;
}

template<typename OutputIt, typename InputIt>
OutputIt copy(OutputIt output, InputIt first, InputIt last) {
    while (first != last) *output++ = *first++;
    return output;
}

} /* namespace __regex_detail */

/* ═══════════════════════════════════════════════════════════════
 * sub_match
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt>
class sub_match : public pair<BidirIt, BidirIt> {
public:
    using iterator = BidirIt;
    using value_type = typename iterator_traits<BidirIt>::value_type;
    using difference_type = typename iterator_traits<BidirIt>::difference_type;
    using string_type = basic_string<value_type>;

    bool matched = false;

    difference_type length() const {
        return matched ? std::distance(this->first, this->second) : 0;
    }

    operator string_type() const {
        return matched ? string_type(this->first, this->second) : string_type();
    }

    string_type str() const {
        return matched ? string_type(this->first, this->second) : string_type();
    }

    int compare(const sub_match& other) const {
        return str().compare(other.str());
    }

    int compare(const string_type& s) const {
        return str().compare(s);
    }

    int compare(const value_type* s) const {
        return str().compare(s);
    }

    void swap(sub_match& other)
        noexcept(is_nothrow_swappable<iterator>::value) {
        using std::swap;
        swap(this->first, other.first);
        swap(this->second, other.second);
        swap(matched, other.matched);
    }
};

template<typename BidirIt>
bool operator==(const sub_match<BidirIt>& left,
                const sub_match<BidirIt>& right) {
    return left.compare(right) == 0;
}

#if __cplusplus >= 202002L
template<typename BidirIt>
strong_ordering operator<=>(const sub_match<BidirIt>& left,
                            const sub_match<BidirIt>& right) {
    int comparison = left.compare(right);
    return comparison < 0 ? strong_ordering::less
         : comparison > 0 ? strong_ordering::greater
                          : strong_ordering::equal;
}
#endif

template<typename BidirIt, typename StringTraits, typename Allocator>
bool operator==(
    const sub_match<BidirIt>& left,
    const basic_string<typename iterator_traits<BidirIt>::value_type,
                       StringTraits, Allocator>& right) {
    using canonical_string = typename sub_match<BidirIt>::string_type;
    return left.compare(canonical_string(right.data(), right.size())) == 0;
}

#if __cplusplus >= 202002L
template<typename BidirIt, typename StringTraits, typename Allocator>
strong_ordering operator<=>(
    const sub_match<BidirIt>& left,
    const basic_string<typename iterator_traits<BidirIt>::value_type,
                       StringTraits, Allocator>& right) {
    using canonical_string = typename sub_match<BidirIt>::string_type;
    int comparison = left.compare(
        canonical_string(right.data(), right.size()));
    return comparison < 0 ? strong_ordering::less
         : comparison > 0 ? strong_ordering::greater
                          : strong_ordering::equal;
}
#endif

template<typename BidirIt>
bool operator==(
    const sub_match<BidirIt>& left,
    const typename iterator_traits<BidirIt>::value_type* right) {
    return left.compare(right) == 0;
}

#if __cplusplus >= 202002L
template<typename BidirIt>
strong_ordering operator<=>(
    const sub_match<BidirIt>& left,
    const typename iterator_traits<BidirIt>::value_type* right) {
    int comparison = left.compare(right);
    return comparison < 0 ? strong_ordering::less
         : comparison > 0 ? strong_ordering::greater
                          : strong_ordering::equal;
}
#endif

template<typename BidirIt>
bool operator==(
    const sub_match<BidirIt>& left,
    const typename iterator_traits<BidirIt>::value_type& right) {
    using string_type = typename sub_match<BidirIt>::string_type;
    return left.compare(string_type(1u, right)) == 0;
}

#if __cplusplus >= 202002L
template<typename BidirIt>
strong_ordering operator<=>(
    const sub_match<BidirIt>& left,
    const typename iterator_traits<BidirIt>::value_type& right) {
    using string_type = typename sub_match<BidirIt>::string_type;
    int comparison = left.compare(string_type(1u, right));
    return comparison < 0 ? strong_ordering::less
         : comparison > 0 ? strong_ordering::greater
                          : strong_ordering::equal;
}
#endif

template<typename CharT, typename StreamTraits, typename BidirIt>
basic_ostream<CharT, StreamTraits>& operator<<(
    basic_ostream<CharT, StreamTraits>& stream,
    const sub_match<BidirIt>& match) {
    return stream << match.str();
}

template<typename BidirIt>
void swap(sub_match<BidirIt>& left, sub_match<BidirIt>& right)
    noexcept(noexcept(left.swap(right))) {
    left.swap(right);
}

using csub_match = sub_match<const char*>;
using ssub_match = sub_match<string::const_iterator>;
using wcsub_match = sub_match<const wchar_t*>;
using wssub_match = sub_match<wstring::const_iterator>;

/* ═══════════════════════════════════════════════════════════════
 * match_results
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt,
         typename Alloc = allocator<sub_match<BidirIt>>>
class match_results {
public:
    using value_type = sub_match<BidirIt>;
    using const_reference = const value_type&;
    using reference = value_type&;
    using const_iterator = const value_type*;
    using iterator = const_iterator;
    using difference_type =
        typename iterator_traits<BidirIt>::difference_type;
    using allocator_type = Alloc;
    using allocator_traits_type = allocator_traits<allocator_type>;
    using size_type = typename allocator_traits_type::size_type;
    using char_type = typename iterator_traits<BidirIt>::value_type;
    using string_type = basic_string<char_type>;

    match_results() : match_results(allocator_type()) {}

    explicit match_results(const allocator_type& selected) noexcept
        : ready_(false), matched_(false), match_count_(0u), base_(),
          allocator_(selected) {}

    match_results(const match_results& other)
        : match_results(
              allocator_traits_type::select_on_container_copy_construction(
                  other.allocator_)) {
        copy_state(other);
    }

    match_results(const match_results& other,
                  const allocator_type& selected)
        : match_results(selected) {
        copy_state(other);
    }

    match_results(match_results&& other) noexcept
        : ready_(false), matched_(false), match_count_(0u), base_(),
          allocator_(std::move(other.allocator_)) {
        copy_state(other);
    }

    match_results(match_results&& other,
                  const allocator_type& selected)
        : match_results(selected) {
        copy_state(other);
    }

    match_results& operator=(const match_results& other) {
        if (this != &other) {
            if (allocator_traits_type::
                    propagate_on_container_copy_assignment::value)
                allocator_ = other.allocator_;
            copy_state(other);
        }
        return *this;
    }

    match_results& operator=(match_results&& other) {
        if (this != &other) {
            if (allocator_traits_type::
                    propagate_on_container_move_assignment::value)
                allocator_ = std::move(other.allocator_);
            copy_state(other);
        }
        return *this;
    }

    bool ready() const { return ready_; }
    bool empty() const { return size() == 0; }
    size_type size() const { return ready_ && matched_ ? match_count_ : 0u; }
    size_type max_size() const {
        return __regex_detail::maximum_capture_count + 1u;
    }

    difference_type length(size_type n = 0) const {
        return n < size() ? matches_[n].length() : 0;
    }
    difference_type position(size_type n = 0) const {
        return n < size() ? std::distance(base_, matches_[n].first)
                          : difference_type(-1);
    }
    string_type str(size_type n = 0) const {
        return n < size() ? matches_[n].str() : string_type();
    }

    const_reference operator[](size_type n) const {
        return n < size() ? matches_[n] : unmatched_result();
    }

    const_reference prefix() const {
        return matched_ ? prefix_ : unmatched_result();
    }

    const_reference suffix() const {
        return matched_ ? suffix_ : unmatched_result();
    }

    const_iterator begin() const { return matches_; }
    const_iterator end() const { return matches_ + size(); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    template<typename OutputIt>
    OutputIt format(OutputIt out, const char_type* fmt_first,
                   const char_type* fmt_last,
                   regex_constants::match_flag_type flags = regex_constants::format_default) const {
        const bool sed = (flags & regex_constants::format_sed) != 0;
        while (fmt_first != fmt_last) {
            char_type value = *fmt_first++;
            if (sed && value == char_type('&')) {
                out = __regex_detail::copy(out, matches_[0].first,
                                           matches_[0].second);
                continue;
            }
            if (sed && value == char_type('\\') && fmt_first != fmt_last) {
                char_type escaped = *fmt_first;
                if (escaped == char_type('&') || escaped == char_type('\\')) {
                    *out++ = escaped;
                    ++fmt_first;
                    continue;
                }
                if (escaped >= char_type('1') && escaped <= char_type('9')) {
                    size_type capture = static_cast<size_type>(
                        escaped - char_type('0'));
                    if (capture < size() && matches_[capture].matched)
                        out = __regex_detail::copy(
                            out, matches_[capture].first,
                            matches_[capture].second);
                    ++fmt_first;
                    continue;
                }
            }
            if (!sed && value == char_type('$') && fmt_first != fmt_last) {
                char_type selector = *fmt_first;
                if (selector == char_type('$')) {
                    *out++ = char_type('$');
                    ++fmt_first;
                    continue;
                }
                if (selector == char_type('&')) {
                    out = __regex_detail::copy(out, matches_[0].first,
                                               matches_[0].second);
                    ++fmt_first;
                    continue;
                }
                if (selector == char_type('`')) {
                    out = __regex_detail::copy(out, prefix_.first,
                                               prefix_.second);
                    ++fmt_first;
                    continue;
                }
                if (selector == char_type('\'')) {
                    out = __regex_detail::copy(out, suffix_.first,
                                               suffix_.second);
                    ++fmt_first;
                    continue;
                }
                if (selector >= char_type('1') && selector <= char_type('9')) {
                    size_type capture = static_cast<size_type>(
                        selector - char_type('0'));
                    if (fmt_first + 1 != fmt_last &&
                        fmt_first[1] >= char_type('0') &&
                        fmt_first[1] <= char_type('9')) {
                        capture = capture * 10u + static_cast<size_type>(
                            fmt_first[1] - char_type('0'));
                        ++fmt_first;
                    }
                    if (capture < size() && matches_[capture].matched)
                        out = __regex_detail::copy(
                            out, matches_[capture].first,
                            matches_[capture].second);
                    ++fmt_first;
                    continue;
                }
            }
            *out++ = value;
        }
        return out;
    }

    template<typename OutputIt, typename StringTraits, typename Allocator2>
    OutputIt format(
        OutputIt out,
        const basic_string<char_type, StringTraits, Allocator2>& fmt,
        regex_constants::match_flag_type flags =
            regex_constants::format_default) const {
        return format(out, fmt.data(), fmt.data() + fmt.size(), flags);
    }

    template<typename StringTraits, typename Allocator2>
    basic_string<char_type, StringTraits, Allocator2> format(
        const basic_string<char_type, StringTraits, Allocator2>& fmt,
        regex_constants::match_flag_type flags =
            regex_constants::format_default) const {
        basic_string<char_type, StringTraits, Allocator2> output;
        format(back_inserter(output), fmt, flags);
        return output;
    }

    string_type format(
        const char_type* fmt,
        regex_constants::match_flag_type flags =
            regex_constants::format_default) const {
        string_type output;
        if (!fmt) return output;
        const char_type* last = fmt;
        while (*last != char_type()) ++last;
        format(back_inserter(output), fmt, last, flags);
        return output;
    }

    allocator_type get_allocator() const { return allocator_; }

    void swap(match_results& other)
        noexcept(is_nothrow_swappable<BidirIt>::value &&
            (!allocator_traits_type::propagate_on_container_swap::value ||
             is_nothrow_swappable<allocator_type>::value)) {
        using std::swap;
        if (allocator_traits_type::
                propagate_on_container_swap::value)
            swap(allocator_, other.allocator_);
        swap(ready_, other.ready_);
        swap(matched_, other.matched_);
        swap(match_count_, other.match_count_);
        for (size_type index = 0u; index < max_size(); ++index)
            swap(matches_[index], other.matches_[index]);
        swap(prefix_, other.prefix_);
        swap(suffix_, other.suffix_);
        swap(base_, other.base_);
    }

    void __set_unmatched() {
        ready_ = true;
        matched_ = false;
        match_count_ = 0u;
        for (size_type index = 0u; index < max_size(); ++index)
            matches_[index] = value_type();
        prefix_ = value_type();
        suffix_ = value_type();
        base_ = BidirIt();
    }

    void __set_match(BidirIt range_first, BidirIt range_last,
                     BidirIt match_first, BidirIt match_last,
                     const __regex_detail::capture_set<BidirIt>& captures,
                     size_type capture_count) {
        ready_ = true;
        matched_ = true;
        match_count_ = capture_count + 1u;
        base_ = range_first;
        for (size_type index = 0u; index < max_size(); ++index)
            matches_[index] = value_type();
        matches_[0].first = match_first;
        matches_[0].second = match_last;
        matches_[0].matched = true;
        for (size_type index = 0u; index < capture_count; ++index) {
            if (captures.values[index].matched) {
                matches_[index + 1u].first = captures.values[index].first;
                matches_[index + 1u].second = captures.values[index].second;
                matches_[index + 1u].matched = true;
            } else {
                matches_[index + 1u].first = range_last;
                matches_[index + 1u].second = range_last;
            }
        }
        prefix_.first = range_first;
        prefix_.second = match_first;
        prefix_.matched = range_first != match_first;
        suffix_.first = match_last;
        suffix_.second = range_last;
        suffix_.matched = match_last != range_last;
    }

    void __set_iteration_context(BidirIt original_first,
                                 BidirIt prefix_first) {
        base_ = original_first;
        prefix_.first = prefix_first;
        prefix_.matched = prefix_.first != prefix_.second;
    }

private:
    bool ready_;
    bool matched_;
    size_type match_count_;
    value_type matches_[__regex_detail::maximum_capture_count + 1u];
    value_type prefix_;
    value_type suffix_;
    BidirIt base_;
    [[no_unique_address]] allocator_type allocator_;

    void copy_state(const match_results& other) {
        ready_ = other.ready_;
        matched_ = other.matched_;
        match_count_ = other.match_count_;
        for (size_type index = 0u; index < max_size(); ++index)
            matches_[index] = other.matches_[index];
        prefix_ = other.prefix_;
        suffix_ = other.suffix_;
        base_ = other.base_;
    }

    static const_reference unmatched_result() {
        static const value_type unmatched{};
        return unmatched;
    }
};

using cmatch = match_results<const char*>;
using smatch = match_results<string::const_iterator>;
using wcmatch = match_results<const wchar_t*>;
using wsmatch = match_results<wstring::const_iterator>;

template<typename BidirIt, typename Allocator>
bool operator==(const match_results<BidirIt, Allocator>& left,
                const match_results<BidirIt, Allocator>& right) {
    if (left.ready() != right.ready()) return false;
    if (!left.ready()) return true;
    if (left.empty() || right.empty())
        return left.empty() && right.empty();
    if (!(left.prefix() == right.prefix()) ||
        left.size() != right.size() ||
        !(left.suffix() == right.suffix()))
        return false;
    for (typename match_results<BidirIt, Allocator>::size_type index = 0u;
         index < left.size(); ++index) {
        if (!(left[index] == right[index])) return false;
    }
    return true;
}

template<typename BidirIt, typename Allocator>
void swap(match_results<BidirIt, Allocator>& left,
          match_results<BidirIt, Allocator>& right)
    noexcept(noexcept(left.swap(right))) {
    left.swap(right);
}

/* ═══════════════════════════════════════════════════════════════
 * regex_match
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt, typename Alloc, typename CharT, typename RxTraits>
bool regex_match(BidirIt first, BidirIt last,
                match_results<BidirIt, Alloc>& m,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    BidirIt matched_last = first;
    __regex_detail::capture_set<BidirIt> captures;
    if (!e.__valid() || !__regex_detail::valid_match_flags(flags)) {
        m.__set_unmatched();
        return false;
    }
    bool complexity_exceeded = false;
    const bool matched = __regex_detail::match_at(
        first, last, first, e, flags, matched_last, captures, true,
        &complexity_exceeded);
    if (complexity_exceeded) {
        m.__set_unmatched();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        __regex_detail::complexity_fail();
#else
        return false;
#endif
    }
    if (!matched || matched_last != last) {
        m.__set_unmatched();
        return false;
    }
    m.__set_match(first, last, first, matched_last, captures, e.mark_count());
    return true;
}

template<typename BidirIt, typename CharT, typename RxTraits>
bool regex_match(BidirIt first, BidirIt last,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<BidirIt> m;
    return regex_match(first, last, m, e, flags);
}

template<typename CharT, typename Alloc, typename RxTraits>
bool regex_match(const CharT* str,
                match_results<const CharT*, Alloc>& m,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    const CharT* end = str;
    while (*end) ++end;
    return regex_match(str, end, m, e, flags);
}

template<typename CharT, typename RxTraits>
bool regex_match(const CharT* str,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<const CharT*> m;
    return regex_match(str, m, e, flags);
}

template<typename CharT, typename Traits, typename Alloc, typename Alloc2,
         typename RxTraits>
bool regex_match(const basic_string<CharT, Traits, Alloc>& s,
                match_results<typename basic_string<CharT, Traits, Alloc>::const_iterator, Alloc2>& m,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_match(s.begin(), s.end(), m, e, flags);
}

template<typename CharT, typename Traits, typename Alloc, typename RxTraits>
bool regex_match(const basic_string<CharT, Traits, Alloc>& s,
                const basic_regex<CharT, RxTraits>& e,
                regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<typename basic_string<CharT, Traits, Alloc>::const_iterator> m;
    return regex_match(s.begin(), s.end(), m, e, flags);
}

/* ═══════════════════════════════════════════════════════════════
 * regex_search
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt, typename Alloc, typename CharT, typename RxTraits>
bool regex_search(BidirIt first, BidirIt last,
                 match_results<BidirIt, Alloc>& m,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    if (!e.__valid() || !__regex_detail::valid_match_flags(flags)) {
        m.__set_unmatched();
        return false;
    }
    BidirIt candidate = first;
    __regex_detail::capture_set<BidirIt> captures;
    for (;;) {
        BidirIt matched_last = candidate;
        bool complexity_exceeded = false;
        const bool matched = __regex_detail::match_at(
            first, last, candidate, e, flags, matched_last, captures, false,
            &complexity_exceeded);
        if (complexity_exceeded) {
            m.__set_unmatched();
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            __regex_detail::complexity_fail();
#else
            return false;
#endif
        }
        if (matched) {
            m.__set_match(first, last, candidate, matched_last, captures,
                          e.mark_count());
            return true;
        }
        if ((flags & regex_constants::match_continuous) != 0 ||
            candidate == last) break;
        ++candidate;
    }
    m.__set_unmatched();
    return false;
}

template<typename BidirIt, typename CharT, typename RxTraits>
bool regex_search(BidirIt first, BidirIt last,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<BidirIt> m;
    return regex_search(first, last, m, e, flags);
}

template<typename CharT, typename Alloc, typename RxTraits>
bool regex_search(const CharT* str,
                 match_results<const CharT*, Alloc>& m,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    const CharT* end = str;
    while (*end) ++end;
    return regex_search(str, end, m, e, flags);
}

template<typename CharT, typename RxTraits>
bool regex_search(const CharT* str,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<const CharT*> m;
    return regex_search(str, m, e, flags);
}

template<typename CharT, typename Traits, typename Alloc, typename Alloc2,
         typename RxTraits>
bool regex_search(const basic_string<CharT, Traits, Alloc>& s,
                 match_results<typename basic_string<CharT, Traits, Alloc>::const_iterator, Alloc2>& m,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_search(s.begin(), s.end(), m, e, flags);
}

template<typename CharT, typename Traits, typename Alloc, typename RxTraits>
bool regex_search(const basic_string<CharT, Traits, Alloc>& s,
                 const basic_regex<CharT, RxTraits>& e,
                 regex_constants::match_flag_type flags = regex_constants::match_default) {
    match_results<typename basic_string<CharT, Traits, Alloc>::const_iterator> m;
    return regex_search(s.begin(), s.end(), m, e, flags);
}

/* ═══════════════════════════════════════════════════════════════
 * regex_iterator
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt,
         typename CharT = typename iterator_traits<BidirIt>::value_type,
         typename RxTraits = regex_traits<CharT>>
class regex_iterator {
public:
    using regex_type = basic_regex<CharT, RxTraits>;
    using value_type = match_results<BidirIt>;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = forward_iterator_tag;

    regex_iterator()
        : begin_(), end_(), expression_(nullptr), flags_(), match_() {}

    regex_iterator(BidirIt first, BidirIt last, const regex_type& expression,
                   regex_constants::match_flag_type flags =
                       regex_constants::match_default)
        : begin_(first), end_(last), expression_(&expression), flags_(flags),
          match_() {
        if (!regex_search(begin_, end_, match_, *expression_, flags_))
            expression_ = nullptr;
    }

    regex_iterator(BidirIt, BidirIt, const regex_type&&,
                   regex_constants::match_flag_type =
                       regex_constants::match_default) = delete;

    regex_iterator(const regex_iterator&) = default;
    regex_iterator(regex_iterator&&) = default;
    regex_iterator& operator=(const regex_iterator&) = default;
    regex_iterator& operator=(regex_iterator&&) = default;

    reference operator*() const { return match_; }
    pointer operator->() const { return &match_; }

    regex_iterator& operator++() {
        if (!expression_ || !match_[0].matched) return *this;

        BidirIt start = match_[0].second;
        const BidirIt prefix_first = match_[0].second;
        if (match_[0].first == match_[0].second) {
            if (start == end_) {
                expression_ = nullptr;
                return *this;
            }
            const regex_constants::match_flag_type continuous_nonempty =
                flags_ | regex_constants::match_not_null |
                regex_constants::match_continuous;
            if (regex_search(start, end_, match_, *expression_,
                             continuous_nonempty)) {
                match_.__set_iteration_context(begin_, prefix_first);
                return *this;
            }
            ++start;
        }

        flags_ |= regex_constants::match_prev_avail;
        if (regex_search(start, end_, match_, *expression_, flags_)) {
            match_.__set_iteration_context(begin_, prefix_first);
        } else {
            expression_ = nullptr;
        }
        return *this;
    }

    regex_iterator operator++(int) {
        regex_iterator previous(*this);
        ++(*this);
        return previous;
    }

    bool operator==(const regex_iterator& other) const {
        if (!expression_ && !other.expression_) return true;
        if (!expression_ || !other.expression_) return false;
        const value_type& left = match_;
        const value_type& right = other.match_;
        return expression_ == other.expression_ && begin_ == other.begin_ &&
               end_ == other.end_ && flags_ == other.flags_ &&
               left[0].matched == right[0].matched &&
               left[0].first == right[0].first &&
               left[0].second == right[0].second;
    }

    bool operator!=(const regex_iterator& other) const {
        return !(*this == other);
    }

private:
    BidirIt begin_;
    BidirIt end_;
    const regex_type* expression_;
    regex_constants::match_flag_type flags_;
    value_type match_;
};

using cregex_iterator = regex_iterator<const char*>;
using sregex_iterator = regex_iterator<string::const_iterator>;
using wcregex_iterator = regex_iterator<const wchar_t*>;
using wsregex_iterator = regex_iterator<wstring::const_iterator>;

/* ═══════════════════════════════════════════════════════════════
 * regex_token_iterator
 * ═══════════════════════════════════════════════════════════════*/

template<typename BidirIt,
         typename CharT = typename iterator_traits<BidirIt>::value_type,
         typename RxTraits = regex_traits<CharT>>
class regex_token_iterator {
public:
    using regex_type = basic_regex<CharT, RxTraits>;
    using value_type = sub_match<BidirIt>;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = forward_iterator_tag;

    regex_token_iterator()
        : position_(), submatches_(), suffix_(), submatch_index_(0u),
          result_(nullptr), has_suffix_selector_(false) {}

    regex_token_iterator(BidirIt first, BidirIt last,
                         const regex_type& expression, int submatch = 0,
                         regex_constants::match_flag_type flags =
                             regex_constants::match_default)
        : position_(first, last, expression, flags), submatches_(), suffix_(),
          submatch_index_(0u), result_(nullptr),
          has_suffix_selector_(submatch == -1) {
        submatches_.push_back(submatch);
        initialize(first, last);
    }

    regex_token_iterator(BidirIt first, BidirIt last,
                         const regex_type& expression,
                         const vector<int>& submatches,
                         regex_constants::match_flag_type flags =
                             regex_constants::match_default)
        : position_(first, last, expression, flags), submatches_(submatches),
          suffix_(), submatch_index_(0u), result_(nullptr),
          has_suffix_selector_(contains_suffix_selector()) {
        initialize(first, last);
    }

    regex_token_iterator(BidirIt first, BidirIt last,
                         const regex_type& expression,
                         initializer_list<int> submatches,
                         regex_constants::match_flag_type flags =
                             regex_constants::match_default)
        : position_(first, last, expression, flags), submatches_(submatches),
          suffix_(), submatch_index_(0u), result_(nullptr),
          has_suffix_selector_(contains_suffix_selector()) {
        initialize(first, last);
    }

    template<size_t Count>
    regex_token_iterator(BidirIt first, BidirIt last,
                         const regex_type& expression,
                         const int (&submatches)[Count],
                         regex_constants::match_flag_type flags =
                             regex_constants::match_default)
        : position_(first, last, expression, flags),
          submatches_(submatches, submatches + Count), suffix_(),
          submatch_index_(0u), result_(nullptr),
          has_suffix_selector_(contains_suffix_selector()) {
        initialize(first, last);
    }

    regex_token_iterator(BidirIt, BidirIt, const regex_type&&, int = 0,
                         regex_constants::match_flag_type =
                             regex_constants::match_default) = delete;
    regex_token_iterator(BidirIt, BidirIt, const regex_type&&,
                         const vector<int>&,
                         regex_constants::match_flag_type =
                             regex_constants::match_default) = delete;
    regex_token_iterator(BidirIt, BidirIt, const regex_type&&,
                         initializer_list<int>,
                         regex_constants::match_flag_type =
                             regex_constants::match_default) = delete;
    template<size_t Count>
    regex_token_iterator(BidirIt, BidirIt, const regex_type&&,
                         const int (&)[Count],
                         regex_constants::match_flag_type =
                             regex_constants::match_default) = delete;

    regex_token_iterator(const regex_token_iterator& other)
        : position_(other.position_), submatches_(other.submatches_),
          suffix_(other.suffix_), submatch_index_(other.submatch_index_),
          result_(nullptr),
          has_suffix_selector_(other.has_suffix_selector_) {
        normalize_result();
    }

    regex_token_iterator(regex_token_iterator&& other)
        : regex_token_iterator(static_cast<const regex_token_iterator&>(other)) {}

    regex_token_iterator& operator=(const regex_token_iterator& other) {
        if (this == &other) return *this;
        position_ = other.position_;
        submatches_ = other.submatches_;
        suffix_ = other.suffix_;
        submatch_index_ = other.submatch_index_;
        has_suffix_selector_ = other.has_suffix_selector_;
        normalize_result();
        return *this;
    }

    regex_token_iterator& operator=(regex_token_iterator&& other) {
        return *this = static_cast<const regex_token_iterator&>(other);
    }

    reference operator*() const { return *result_; }
    pointer operator->() const { return result_; }

    regex_token_iterator& operator++() {
        if (!result_) return *this;
        if (suffix_.matched) {
            *this = regex_token_iterator();
            return *this;
        }
        if (submatch_index_ + 1u < submatches_.size()) {
            ++submatch_index_;
            result_ = &current_match();
            return *this;
        }

        regex_iterator<BidirIt, CharT, RxTraits> previous = position_;
        submatch_index_ = 0u;
        ++position_;
        if (position_ != regex_iterator<BidirIt, CharT, RxTraits>()) {
            result_ = &current_match();
        } else if (has_suffix_selector_ &&
                   previous->suffix().length() != 0) {
            suffix_.matched = true;
            suffix_.first = previous->suffix().first;
            suffix_.second = previous->suffix().second;
            result_ = &suffix_;
        } else {
            *this = regex_token_iterator();
        }
        return *this;
    }

    regex_token_iterator operator++(int) {
        regex_token_iterator previous(*this);
        ++(*this);
        return previous;
    }

    bool operator==(const regex_token_iterator& other) const {
        if (!result_ && !other.result_) return true;
        if (suffix_.matched && other.suffix_.matched)
            return same_submatch(suffix_, other.suffix_);
        if (!result_ || suffix_.matched || !other.result_ ||
            other.suffix_.matched)
            return false;
        return position_ == other.position_ &&
               submatch_index_ == other.submatch_index_ &&
               submatches_ == other.submatches_;
    }

    bool operator!=(const regex_token_iterator& other) const {
        return !(*this == other);
    }

private:
    using position_type = regex_iterator<BidirIt, CharT, RxTraits>;

    position_type position_;
    vector<int> submatches_;
    value_type suffix_;
    size_t submatch_index_;
    const value_type* result_;
    bool has_suffix_selector_;

    bool contains_suffix_selector() const {
        for (size_t index = 0u; index < submatches_.size(); ++index) {
            if (submatches_[index] == -1) return true;
        }
        return false;
    }

    const value_type& current_match() const {
        int selector = submatches_[submatch_index_];
        return selector == -1
            ? position_->prefix()
            : (*position_)[static_cast<size_t>(selector)];
    }

    void initialize(BidirIt first, BidirIt last) {
        if (submatches_.empty()) {
            result_ = nullptr;
        } else if (position_ != position_type()) {
            result_ = &current_match();
        } else if (has_suffix_selector_) {
            suffix_.matched = true;
            suffix_.first = first;
            suffix_.second = last;
            result_ = &suffix_;
        }
    }

    void normalize_result() {
        if (suffix_.matched) {
            result_ = &suffix_;
        } else if (!submatches_.empty() && position_ != position_type()) {
            result_ = &current_match();
        } else {
            result_ = nullptr;
        }
    }

    static bool same_submatch(const value_type& left,
                              const value_type& right) {
        return left.matched == right.matched && left.first == right.first &&
               left.second == right.second;
    }
};

using cregex_token_iterator = regex_token_iterator<const char*>;
using sregex_token_iterator = regex_token_iterator<string::const_iterator>;
using wcregex_token_iterator = regex_token_iterator<const wchar_t*>;
using wsregex_token_iterator = regex_token_iterator<wstring::const_iterator>;

/* ═══════════════════════════════════════════════════════════════
 * regex_replace
 * ═══════════════════════════════════════════════════════════════*/

template<typename OutputIt, typename BidirIt, typename CharT,
         typename RxTraits>
OutputIt regex_replace(OutputIt out,
                      BidirIt first, BidirIt last,
                      const basic_regex<CharT, RxTraits>& e,
                      const CharT* fmt,
                      regex_constants::match_flag_type flags = regex_constants::match_default) {
    const regex_constants::match_flag_type supported_flags =
        regex_constants::match_not_bol | regex_constants::match_not_eol |
        regex_constants::match_not_bow | regex_constants::match_not_eow |
        regex_constants::match_any | regex_constants::match_not_null |
        regex_constants::match_continuous |
        regex_constants::match_prev_avail | regex_constants::format_sed |
        regex_constants::format_no_copy |
        regex_constants::format_first_only;
    if (!fmt || !e.__valid() || (flags & ~supported_flags) != 0) return out;
    const CharT* fmt_last = fmt;
    while (*fmt_last != CharT()) ++fmt_last;
    const bool no_copy = (flags & regex_constants::format_no_copy) != 0;
    const bool first_only = (flags & regex_constants::format_first_only) != 0;
    regex_constants::match_flag_type match_flags = flags &
        (regex_constants::match_not_bol | regex_constants::match_not_eol |
         regex_constants::match_not_bow | regex_constants::match_not_eow |
         regex_constants::match_any | regex_constants::match_not_null |
         regex_constants::match_continuous |
         regex_constants::match_prev_avail);
    BidirIt current = first;
    match_results<BidirIt> match;
    while (regex_search(current, last, match, e, match_flags)) {
        if (!no_copy) {
            out = __regex_detail::copy(out, match.prefix().first,
                                       match.prefix().second);
        }
        out = match.format(out, fmt, fmt_last, flags);
        BidirIt next = match[0].second;
        bool empty_match = match[0].first == match[0].second;
        if (first_only) {
            if (!no_copy)
                out = __regex_detail::copy(out, next, last);
            return out;
        }
        if (empty_match) {
            current = next;
            if (current == last) return out;
            if (!no_copy) *out++ = *current;
            ++current;
        } else {
            current = next;
        }
        match_flags |= regex_constants::match_prev_avail;
    }
    if (!no_copy) out = __regex_detail::copy(out, current, last);
    return out;
}

template<typename OutputIt, typename BidirIt, typename CharT,
         typename RxTraits, typename Traits, typename Alloc>
OutputIt regex_replace(OutputIt out,
                      BidirIt first, BidirIt last,
                      const basic_regex<CharT, RxTraits>& e,
                      const basic_string<CharT, Traits, Alloc>& fmt,
                      regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_replace(out, first, last, e, fmt.c_str(), flags);
}

template<typename CharT, typename Traits, typename Alloc, typename RxTraits>
basic_string<CharT, Traits, Alloc> regex_replace(
    const basic_string<CharT, Traits, Alloc>& s,
    const basic_regex<CharT, RxTraits>& e,
    const CharT* fmt,
    regex_constants::match_flag_type flags = regex_constants::match_default) {
    basic_string<CharT, Traits, Alloc> result;
    regex_replace(back_inserter(result), s.begin(), s.end(), e, fmt, flags);
    return result;
}

template<typename CharT, typename Traits, typename Alloc, typename RxTraits>
basic_string<CharT, Traits, Alloc> regex_replace(
    const basic_string<CharT, Traits, Alloc>& s,
    const basic_regex<CharT, RxTraits>& e,
    const basic_string<CharT, Traits, Alloc>& fmt,
    regex_constants::match_flag_type flags = regex_constants::match_default) {
    return regex_replace(s, e, fmt.c_str(), flags);
}

template<typename CharT, typename RxTraits>
basic_string<CharT> regex_replace(
    const CharT* s,
    const basic_regex<CharT, RxTraits>& e,
    const CharT* fmt,
    regex_constants::match_flag_type flags = regex_constants::match_default) {
    basic_string<CharT> str(s);
    return regex_replace(str, e, fmt, flags);
}

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_REGEX_H */
