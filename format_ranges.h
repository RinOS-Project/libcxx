/*
 * RinOS C++23 range formatting support for <format>.
 */

#ifndef RINCXX_FORMAT_RANGES_H
#define RINCXX_FORMAT_RANGES_H

#if defined(__cplusplus) && __cplusplus > 202002L

#include "ranges.h"
#include "tuple.h"
#include "map.h"
#include "set.h"
#include "unordered_map.h"
#include "unordered_set.h"

namespace std {

template<typename T, typename CharT = char>
concept formattable =
    is_default_constructible_v<formatter<remove_cvref_t<T>, CharT>> &&
    requires(formatter<remove_cvref_t<T>, CharT>& selected, T&& value,
             basic_format_context<detail::format_output_iterator<CharT>, CharT>& context) {
        selected.format(static_cast<T&&>(value), context);
    };

enum class range_format : unsigned char {
    disabled,
    map,
    set,
    sequence,
    string,
    debug_string
};

template<typename R>
inline constexpr range_format format_kind = [] {
    if constexpr (ranges::input_range<R>) {
        using reference = ranges::range_reference_t<R>;
        if constexpr (!is_same_v<remove_cvref_t<reference>, remove_cvref_t<R>> &&
                      formattable<reference, char>) {
            return range_format::sequence;
        }
    }
    return range_format::disabled;
}();

template<typename CharT, typename Traits, typename Allocator>
inline constexpr range_format
    format_kind<basic_string<CharT, Traits, Allocator>> = range_format::string;
template<typename CharT, typename Traits>
inline constexpr range_format
    format_kind<basic_string_view<CharT, Traits>> = range_format::string;

template<typename Key, typename Value, typename Compare, typename Allocator>
inline constexpr range_format format_kind<map<Key, Value, Compare, Allocator>> =
    range_format::map;
template<typename Key, typename Value, typename Compare, typename Allocator>
inline constexpr range_format
    format_kind<multimap<Key, Value, Compare, Allocator>> = range_format::map;
template<typename Key, typename Value, typename Hash, typename Equal,
         typename Allocator>
inline constexpr range_format
    format_kind<unordered_map<Key, Value, Hash, Equal, Allocator>> =
        range_format::map;
template<typename Key, typename Value, typename Hash, typename Equal,
         typename Allocator>
inline constexpr range_format
    format_kind<unordered_multimap<Key, Value, Hash, Equal, Allocator>> =
        range_format::map;
template<typename Key, typename Compare, typename Allocator>
inline constexpr range_format format_kind<set<Key, Compare, Allocator>> =
    range_format::set;
template<typename Key, typename Compare, typename Allocator>
inline constexpr range_format format_kind<multiset<Key, Compare, Allocator>> =
    range_format::set;
template<typename Key, typename Hash, typename Equal, typename Allocator>
inline constexpr range_format
    format_kind<unordered_set<Key, Hash, Equal, Allocator>> = range_format::set;
template<typename Key, typename Hash, typename Equal, typename Allocator>
inline constexpr range_format
    format_kind<unordered_multiset<Key, Hash, Equal, Allocator>> =
        range_format::set;

namespace detail {

template<typename CharT>
constexpr basic_string_view<CharT> format_static_text(
    const char* narrow, const wchar_t* wide, size_t size) {
    if constexpr (is_same_v<CharT, char>)
        return basic_string_view<char>(narrow, size);
    else
        return basic_string_view<wchar_t>(wide, size);
}

template<typename T, typename = void>
struct format_tuple_size_two : false_type {};

template<typename T>
struct format_tuple_size_two<T, void_t<decltype(tuple_size<T>::value)>>
    : bool_constant<tuple_size<T>::value == 2u> {};

template<typename CharT, typename Render>
auto format_buffered_and_aligned(Render&& render,
                                 basic_format_context<
                                     format_output_iterator<CharT>, CharT>& ctx,
                                 size_t width, CharT fill, char alignment) {
    basic_string<CharT> buffer;
    format_output_sink<CharT> sink(buffer, format_max_output_size);
    format_output_iterator<CharT> inner_out(sink);
    basic_format_context<format_output_iterator<CharT>, CharT> inner_context(
        inner_out, ctx.args(), ctx.locale());
    render(inner_context);

    const size_t padding = width > buffer.size() ? width - buffer.size() : 0u;
    size_t left_padding = 0u;
    size_t right_padding = 0u;
    if (alignment == '<') {
        right_padding = padding;
    } else if (alignment == '^') {
        left_padding = padding / 2u;
        right_padding = padding - left_padding;
    } else {
        left_padding = padding;
    }

    auto out = ctx.out();
    for (size_t index = 0u; index < left_padding; ++index) *out++ = fill;
    for (CharT character : buffer) *out++ = character;
    for (size_t index = 0u; index < right_padding; ++index) *out++ = fill;
    return out;
}

template<typename T, typename CharT, size_t Index>
struct tuple_format_entry {
    using element_type = remove_cvref_t<tuple_element_t<Index, T>>;
    formatter<element_type, CharT> value;

    constexpr void set_debug_format() {
        if constexpr (requires { value.set_debug_format(); })
            value.set_debug_format();
    }
};

template<typename T, typename CharT, typename Sequence>
class tuple_like_formatter;

template<typename T, typename CharT, size_t... Indices>
class tuple_like_formatter<T, CharT, index_sequence<Indices...>>
    : private tuple_format_entry<T, CharT, Indices>... {
    basic_string_view<CharT> separator_ =
        format_static_text<CharT>(", ", L", ", 2u);
    basic_string_view<CharT> opening_ =
        format_static_text<CharT>("(", L"(", 1u);
    basic_string_view<CharT> closing_ =
        format_static_text<CharT>(")", L")", 1u);
    size_t width_ = 0u;
    CharT fill_ = static_cast<CharT>(' ');
    char alignment_ = '<';

    template<size_t Index>
    auto& entry() {
        return static_cast<tuple_format_entry<T, CharT, Index>&>(*this);
    }

    template<typename Context, size_t... Is>
    void render_elements(const T& value, Context& context,
                         index_sequence<Is...>) {
        auto out = context.out();
        for (CharT character : opening_) *out++ = character;
        context.advance_to(out);
        size_t written = 0u;
        ((written++ == 0u ? void() : write_separator(context),
          context.advance_to(entry<Is>().value.format(get<Is>(value), context))), ...);
        out = context.out();
        for (CharT character : closing_) *out++ = character;
        context.advance_to(out);
    }

    template<typename Context>
    void write_separator(Context& context) {
        auto out = context.out();
        for (CharT character : separator_) *out++ = character;
        context.advance_to(out);
    }

public:
    constexpr void set_separator(basic_string_view<CharT> separator) {
        separator_ = separator;
    }

    constexpr void set_brackets(basic_string_view<CharT> opening,
                                basic_string_view<CharT> closing) {
        opening_ = opening;
        closing_ = closing;
    }

    constexpr void set_map_format() {
        if constexpr (sizeof...(Indices) == 2u) {
            separator_ = format_static_text<CharT>(": ", L": ", 2u);
            set_brackets({}, {});
        }
    }

    constexpr void set_debug_format() {
        (entry<Indices>().set_debug_format(), ...);
    }

    template<typename ParseContext>
    constexpr auto parse(ParseContext& context) -> typename ParseContext::iterator {
        auto it = context.begin();
        const auto end = context.end();
        if (it != end && (it + 1) != end &&
            (*(it + 1) == static_cast<CharT>('<') ||
             *(it + 1) == static_cast<CharT>('>') ||
             *(it + 1) == static_cast<CharT>('^'))) {
            fill_ = *it++;
        }
        if (it != end && (*it == static_cast<CharT>('<') ||
                          *it == static_cast<CharT>('>') ||
                          *it == static_cast<CharT>('^'))) {
            alignment_ = static_cast<char>(*it++);
        }
        while (it != end && *it >= static_cast<CharT>('0') &&
               *it <= static_cast<CharT>('9')) {
            const size_t digit = static_cast<size_t>(*it - static_cast<CharT>('0'));
            if (width_ > (format_max_conversion_size - digit) / 10u) return it;
            width_ = width_ * 10u + digit;
            ++it;
        }
        if (it != end && *it == static_cast<CharT>('m')) {
            if constexpr (sizeof...(Indices) != 2u) return context.begin();
            set_map_format();
            ++it;
        } else if (it != end && *it == static_cast<CharT>('n')) {
            set_brackets({}, {});
            ++it;
        }
        (entry<Indices>().set_debug_format(), ...);
        return it;
    }

    template<typename Context>
    auto format(const T& value, Context& context) {
        return format_buffered_and_aligned<CharT>(
            [&](auto& inner_context) {
                render_elements(value, inner_context,
                                index_sequence<Indices...>{});
            }, context, width_, fill_, alignment_);
    }
};

template<typename T, typename CharT>
concept map_tuple_formattable = requires(formatter<T, CharT>& value) {
    value.set_map_format();
};

} /* namespace detail */

template<typename T, typename CharT>
    requires formattable<T, CharT>
class range_formatter {
    formatter<T, CharT> underlying_;
    basic_string_view<CharT> separator_ =
        detail::format_static_text<CharT>(", ", L", ", 2u);
    basic_string_view<CharT> opening_ =
        detail::format_static_text<CharT>("[", L"[", 1u);
    basic_string_view<CharT> closing_ =
        detail::format_static_text<CharT>("]", L"]", 1u);
    size_t width_ = 0u;
    CharT fill_ = static_cast<CharT>(' ');
    char alignment_ = '<';
    range_format type_ = range_format::sequence;

    template<typename Context>
    void write_text(Context& context, basic_string_view<CharT> text) const {
        auto out = context.out();
        for (CharT character : text) *out++ = character;
        context.advance_to(out);
    }

public:
    constexpr void set_separator(basic_string_view<CharT> separator) {
        separator_ = separator;
    }
    constexpr void set_brackets(basic_string_view<CharT> opening,
                                basic_string_view<CharT> closing) {
        opening_ = opening;
        closing_ = closing;
    }
    constexpr formatter<T, CharT>& underlying() { return underlying_; }
    constexpr const formatter<T, CharT>& underlying() const { return underlying_; }

    constexpr void set_debug_format(bool enabled = true) {
        if (enabled) {
            if constexpr (requires { underlying_.set_debug_format(); })
                underlying_.set_debug_format();
        }
    }

    template<typename ParseContext>
    constexpr auto parse(ParseContext& context) -> typename ParseContext::iterator {
        auto it = context.begin();
        const auto end = context.end();
        if (it != end && (it + 1) != end &&
            (*(it + 1) == static_cast<CharT>('<') ||
             *(it + 1) == static_cast<CharT>('>') ||
             *(it + 1) == static_cast<CharT>('^'))) {
            fill_ = *it++;
        }
        if (it != end && (*it == static_cast<CharT>('<') ||
                          *it == static_cast<CharT>('>') ||
                          *it == static_cast<CharT>('^'))) {
            alignment_ = static_cast<char>(*it++);
        }
        while (it != end && *it >= static_cast<CharT>('0') &&
               *it <= static_cast<CharT>('9')) {
            const size_t digit = static_cast<size_t>(*it - static_cast<CharT>('0'));
            if (width_ > (format_max_conversion_size - digit) / 10u) return it;
            width_ = width_ * 10u + digit;
            ++it;
        }
        const bool omit_brackets = it != end && *it == static_cast<CharT>('n');
        if (omit_brackets) ++it;
        if (it != end && *it == static_cast<CharT>('m')) {
            if constexpr (!detail::format_tuple_size_two<T>::value ||
                          !detail::map_tuple_formattable<T, CharT>) {
                return context.begin();
            } else {
                type_ = range_format::map;
                separator_ = detail::format_static_text<CharT>(", ", L", ", 2u);
                underlying_.set_map_format();
                ++it;
            }
        } else if (it != end && *it == static_cast<CharT>('s')) {
            if constexpr (!is_same_v<T, CharT>) return context.begin();
            if (omit_brackets) return context.begin();
            type_ = range_format::string;
            ++it;
        } else if (it != end && *it == static_cast<CharT>('?') &&
                   (it + 1) != end && *(it + 1) == static_cast<CharT>('s')) {
            if constexpr (!is_same_v<T, CharT>) return context.begin();
            if (omit_brackets) return context.begin();
            type_ = range_format::debug_string;
            it += 2;
        }
        if (omit_brackets && type_ != range_format::string &&
            type_ != range_format::debug_string) {
            set_brackets({}, {});
        } else if (type_ == range_format::map) {
            if constexpr (detail::map_tuple_formattable<T, CharT>) {
                if (!omit_brackets) {
                    opening_ = detail::format_static_text<CharT>("{", L"{", 1u);
                    closing_ = detail::format_static_text<CharT>("}", L"}", 1u);
                }
            }
        }
        if (type_ == range_format::string ||
            type_ == range_format::debug_string) {
            if (it != end) return context.begin();
            return it;
        }
        if (it != end && *it == static_cast<CharT>(':')) {
            ++it;
            context.advance_to(it);
            return underlying_.parse(context);
        }
        if constexpr (requires { underlying_.set_debug_format(); })
            underlying_.set_debug_format();
        return it;
    }

    template<ranges::input_range R, typename Context>
        requires formattable<ranges::range_reference_t<R>, CharT> &&
                 is_same_v<remove_cvref_t<ranges::range_reference_t<R>>, T>
    auto format(R&& range, Context& context) {
        return detail::format_buffered_and_aligned<CharT>(
            [&](auto& inner_context) {
                if (type_ == range_format::string ||
                    type_ == range_format::debug_string) {
                    basic_string<CharT> value;
                    for (auto&& element : range)
                        value.push_back(static_cast<CharT>(element));
                    if (type_ == range_format::debug_string) {
                        const auto escaped = detail::format_debug_escape(
                            value.data(), value.size());
                        write_text(inner_context, escaped);
                    } else {
                        write_text(inner_context, basic_string_view<CharT>(
                            value.data(), value.size()));
                    }
                    return;
                }
                auto out = inner_context.out();
                for (CharT character : opening_) *out++ = character;
                inner_context.advance_to(out);
                bool first = true;
                for (auto&& element : range) {
                    if (!first) write_text(inner_context, separator_);
                    first = false;
                    inner_context.advance_to(
                        underlying_.format(element, inner_context));
                }
                out = inner_context.out();
                for (CharT character : closing_) *out++ = character;
                inner_context.advance_to(out);
            }, context, width_, fill_, alignment_);
    }
};

template<ranges::input_range R, typename CharT>
    requires (!is_same_v<remove_cvref_t<ranges::range_reference_t<R>>, R>) &&
             formattable<ranges::range_reference_t<R>, CharT> &&
             (format_kind<R> != range_format::disabled)
struct formatter<R, CharT>
    : range_formatter<remove_cvref_t<ranges::range_reference_t<R>>, CharT> {
    using base = range_formatter<
        remove_cvref_t<ranges::range_reference_t<R>>, CharT>;

    constexpr formatter() {
        if constexpr (format_kind<R> == range_format::map) {
            base::set_brackets(
                detail::format_static_text<CharT>("{", L"{", 1u),
                detail::format_static_text<CharT>("}", L"}", 1u));
            if constexpr (detail::map_tuple_formattable<
                              remove_cvref_t<ranges::range_reference_t<R>>,
                              CharT>)
                base::underlying().set_map_format();
        } else if constexpr (format_kind<R> == range_format::set) {
            base::set_brackets(
                detail::format_static_text<CharT>("{", L"{", 1u),
                detail::format_static_text<CharT>("}", L"}", 1u));
        }
    }

    using maybe_const_range = conditional_t<
        ranges::input_range<const R>, const R, R>;

    template<typename Context>
    auto format(maybe_const_range& value, Context& context) {
        return base::format(value, context);
    }
};

template<typename T1, typename T2, typename CharT>
    requires formattable<T1, CharT> && formattable<T2, CharT>
struct formatter<pair<T1, T2>, CharT>
    : detail::tuple_like_formatter<pair<T1, T2>, CharT,
                                   index_sequence<0u, 1u>> {};

template<typename... Ts, typename CharT>
    requires (formattable<Ts, CharT> && ...)
struct formatter<tuple<Ts...>, CharT>
    : detail::tuple_like_formatter<tuple<Ts...>, CharT,
                                   make_index_sequence<sizeof...(Ts)>> {};

namespace detail {

template<typename R>
struct format_mutable_custom<R>
    : bool_constant<ranges::input_range<R> &&
                    !ranges::input_range<const R>> {};

} /* namespace detail */

} /* namespace std */

#endif /* C++23 */
#endif /* RINCXX_FORMAT_RANGES_H */
