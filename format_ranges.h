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
#include "queue.h"
#include "stack.h"
#include "vector.h"

namespace std {

enum class range_format {
    disabled,
    map,
    set,
    sequence,
    string,
    debug_string
};

template<typename T, typename CharT>
concept formattable =
    semiregular<formatter<remove_cvref_t<T>, CharT>> &&
    requires(formatter<remove_cvref_t<T>, CharT> selected,
             const formatter<remove_cvref_t<T>, CharT> const_selected,
             T value,
             basic_format_context<detail::format_output_iterator<CharT>, CharT>&
                 context,
             basic_format_parse_context<CharT>& parse_context) {
        { selected.parse(parse_context) } -> same_as<
            typename basic_format_parse_context<CharT>::iterator>;
        { const_selected.format(value, context) } -> same_as<
            detail::format_output_iterator<CharT>>;
    };

namespace detail {

template<typename T>
struct format_is_pair : false_type {};

template<typename First, typename Second>
struct format_is_pair<pair<First, Second>> : true_type {};

template<typename T>
struct format_is_tuple_pair : false_type {};

template<typename First, typename Second>
struct format_is_tuple_pair<tuple<First, Second>> : true_type {};

template<typename T>
struct format_is_map_element
    : bool_constant<format_is_pair<remove_cvref_t<T>>::value ||
                    format_is_tuple_pair<remove_cvref_t<T>>::value> {};

template<typename R, typename = void>
struct format_has_key_type : false_type {};

template<typename R>
struct format_has_key_type<R, void_t<typename R::key_type>> : true_type {};

template<typename R, typename = void>
struct format_has_mapped_type : false_type {};

template<typename R>
struct format_has_mapped_type<R, void_t<typename R::mapped_type>> : true_type {};

template<typename R, typename CharT>
concept format_const_formattable_range =
    ranges::input_range<const R> &&
    formattable<ranges::range_reference_t<const R>, CharT>;

template<typename R, typename CharT>
using format_maybe_const_range = conditional_t<
    format_const_formattable_range<R, CharT>, const R, R>;

} /* namespace detail */

template<typename R>
inline constexpr range_format format_kind = range_format::disabled;

template<ranges::input_range R>
    requires is_same_v<R, remove_cvref_t<R>>
inline constexpr range_format format_kind<R> = [] {
    using reference = ranges::range_reference_t<R>;
    using element_type = remove_cvref_t<reference>;
    if constexpr (is_same_v<element_type, R>) {
        return range_format::disabled;
    } else if constexpr (detail::format_has_key_type<R>::value) {
        if constexpr (detail::format_has_mapped_type<R>::value &&
                      detail::format_is_map_element<element_type>::value) {
            return range_format::map;
        } else {
            return range_format::set;
        }
    } else {
        return range_format::sequence;
    }
}();

namespace detail {

template<typename CharT>
constexpr basic_string_view<CharT> format_static_text(
    const char* narrow, const wchar_t* wide, size_t size) {
    if constexpr (is_same_v<CharT, char>)
        return basic_string_view<char>(narrow, size);
    else
        return basic_string_view<wchar_t>(wide, size);
}

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
    static constexpr bool const_formattable =
        (formattable<const remove_cvref_t<tuple_element_t<Indices, T>>,
                     CharT> && ...);
    using format_value_type = conditional_t<const_formattable, const T, T>;
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
    constexpr auto& entry() {
        return static_cast<tuple_format_entry<T, CharT, Index>&>(*this);
    }

    template<size_t Index>
    constexpr const auto& entry() const {
        return static_cast<const tuple_format_entry<T, CharT, Index>&>(*this);
    }

    template<typename Context, size_t... Is>
    void render_elements(format_value_type& value, Context& context,
                         index_sequence<Is...>) const {
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
    void write_separator(Context& context) const {
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
            if (*it == static_cast<CharT>('{') ||
                *it == static_cast<CharT>('}') ||
                *it == static_cast<CharT>(':')) {
                return context.begin();
            }
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
            if (width_ > (detail::format_max_conversion_size - digit) / 10u)
                return it;
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
    auto format(format_value_type& value, Context& context) const {
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

template<typename T, typename CharT = char>
    requires is_same_v<remove_cvref_t<T>, T> && formattable<T, CharT>
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
    constexpr void set_separator(basic_string_view<CharT> separator) noexcept {
        separator_ = separator;
    }
    constexpr void set_brackets(basic_string_view<CharT> opening,
                                basic_string_view<CharT> closing) noexcept {
        opening_ = opening;
        closing_ = closing;
    }
    constexpr formatter<T, CharT>& underlying() noexcept { return underlying_; }
    constexpr const formatter<T, CharT>& underlying() const noexcept {
        return underlying_;
    }

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
            if (*it == static_cast<CharT>('{') ||
                *it == static_cast<CharT>('}') ||
                *it == static_cast<CharT>(':')) {
                return context.begin();
            }
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
            if (width_ > (detail::format_max_conversion_size - digit) / 10u)
                return it;
            width_ = width_ * 10u + digit;
            ++it;
        }
        const bool omit_brackets = it != end && *it == static_cast<CharT>('n');
        if (omit_brackets) ++it;
        if (it != end && *it == static_cast<CharT>('m')) {
            if constexpr (!detail::format_is_map_element<T>::value ||
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
        }
        if (it != end && *it == static_cast<CharT>(':')) {
            ++it;
            context.advance_to(it);
            return underlying_.parse(context);
        }
        context.advance_to(it);
        auto parsed = underlying_.parse(context);
        if (parsed == context.end() &&
            type_ != range_format::string &&
            type_ != range_format::debug_string) {
            if constexpr (requires { underlying_.set_debug_format(); })
                underlying_.set_debug_format();
        }
        return parsed;
    }

    template<ranges::input_range R, typename Context>
        requires formattable<ranges::range_reference_t<R>, CharT> &&
                 is_same_v<remove_cvref_t<ranges::range_reference_t<R>>, T>
    auto format(R&& range, Context& context) const {
        return detail::format_buffered_and_aligned<CharT>(
            [&](auto& inner_context) {
                if constexpr (is_same_v<T, CharT>) {
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

namespace detail {

template<range_format Kind, ranges::input_range R, typename CharT>
struct format_range_default_formatter {
private:
    using maybe_const_range = format_maybe_const_range<R, CharT>;
    using element_type = remove_cvref_t<
        ranges::range_reference_t<maybe_const_range>>;
    range_formatter<element_type, CharT> underlying_;

public:
    constexpr format_range_default_formatter() {
        if constexpr (Kind == range_format::map) {
            static_assert(format_is_map_element<element_type>::value,
                          "range_format::map requires pair or tuple<T, U>");
            underlying_.set_brackets(
                format_static_text<CharT>("{", L"{", 1u),
                format_static_text<CharT>("}", L"}", 1u));
            if constexpr (map_tuple_formattable<element_type, CharT>)
                underlying_.underlying().set_map_format();
        } else if constexpr (Kind == range_format::set) {
            underlying_.set_brackets(
                format_static_text<CharT>("{", L"{", 1u),
                format_static_text<CharT>("}", L"}", 1u));
        }
    }

    constexpr void set_separator(basic_string_view<CharT> separator) noexcept {
        underlying_.set_separator(separator);
    }

    constexpr void set_brackets(basic_string_view<CharT> opening,
                                basic_string_view<CharT> closing) noexcept {
        underlying_.set_brackets(opening, closing);
    }

    template<typename ParseContext>
    constexpr auto parse(ParseContext& context)
        -> typename ParseContext::iterator {
        return underlying_.parse(context);
    }

    template<typename Context>
    auto format(maybe_const_range& range, Context& context) const {
        return underlying_.format(range, context);
    }
};

template<range_format Kind, ranges::input_range R, typename CharT>
    requires (Kind == range_format::string ||
              Kind == range_format::debug_string) &&
             is_same_v<remove_cvref_t<ranges::range_reference_t<
                           format_maybe_const_range<R, CharT>>>, CharT>
struct format_range_default_formatter<Kind, R, CharT> {
private:
    using maybe_const_range = format_maybe_const_range<R, CharT>;
    formatter<basic_string<CharT>, CharT> underlying_;

public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& context)
        -> typename ParseContext::iterator {
        auto parsed = underlying_.parse(context);
        if constexpr (Kind == range_format::debug_string)
            underlying_.set_debug_format();
        return parsed;
    }

    template<typename Context>
    auto format(maybe_const_range& range, Context& context) const {
        basic_string<CharT> value;
        for (auto&& element : range)
            value.push_back(static_cast<CharT>(element));
        return underlying_.format(value, context);
    }
};

} /* namespace detail */

template<typename CharT>
struct formatter<detail::vector_bool_reference, CharT>
    : formatter<bool, CharT> {
    using base_type = formatter<bool, CharT>;

    template<typename Context>
    auto format(const detail::vector_bool_reference& value,
                Context& context) const {
        return static_cast<const base_type&>(*this).format(
            static_cast<bool>(value), context);
    }
};

template<ranges::input_range R, typename CharT>
    requires (format_kind<R> != range_format::disabled) &&
             formattable<ranges::range_reference_t<R>, CharT>
struct formatter<R, CharT>
    : detail::format_range_default_formatter<format_kind<R>, R, CharT> {};

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

#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_PRINT)
template<>
inline constexpr bool enable_nonlocking_formatter_optimization<
    detail::vector_bool_reference> = true;

template<class First, class Second>
inline constexpr bool enable_nonlocking_formatter_optimization<
    pair<First, Second>> =
        enable_nonlocking_formatter_optimization<remove_cvref_t<First>> &&
        enable_nonlocking_formatter_optimization<remove_cvref_t<Second>>;

template<class... Elements>
inline constexpr bool enable_nonlocking_formatter_optimization<
    tuple<Elements...>> =
        (enable_nonlocking_formatter_optimization<
             remove_cvref_t<Elements>> && ...);
#endif

namespace detail {

struct format_container_access {
    template<typename T, typename Container>
    static Container& get(queue<T, Container>& value) {
        return value.c;
    }

    template<typename T, typename Container>
    static const Container& get(const queue<T, Container>& value) {
        return value.c;
    }

    template<typename T, typename Container, typename Compare>
    static Container& get(priority_queue<T, Container, Compare>& value) {
        return value.c;
    }

    template<typename T, typename Container, typename Compare>
    static const Container& get(
        const priority_queue<T, Container, Compare>& value) {
        return value.c;
    }

    template<typename T, typename Container>
    static Container& get(stack<T, Container>& value) {
        return value.c;
    }

    template<typename T, typename Container>
    static const Container& get(const stack<T, Container>& value) {
        return value.c;
    }
};

template<typename R, typename CharT>
struct format_mutable_custom<
    R, CharT, enable_if_t<ranges::input_range<R>>>
    : bool_constant<!format_const_formattable_range<R, CharT>> {};

template<typename First, typename Second, typename CharT>
struct format_mutable_custom<pair<First, Second>, CharT, void>
    : bool_constant<!(formattable<const remove_cvref_t<First>, CharT> &&
                      formattable<const remove_cvref_t<Second>, CharT>)> {};

template<typename... Ts, typename CharT>
struct format_mutable_custom<tuple<Ts...>, CharT, void>
    : bool_constant<!(formattable<const remove_cvref_t<Ts>, CharT> && ...)> {};

template<typename T, typename Container, typename CharT>
struct format_mutable_custom<queue<T, Container>, CharT, void> : true_type {};

template<typename T, typename Container, typename Compare, typename CharT>
struct format_mutable_custom<priority_queue<T, Container, Compare>, CharT, void>
    : true_type {};

template<typename T, typename Container, typename CharT>
struct format_mutable_custom<stack<T, Container>, CharT, void> : true_type {};

} /* namespace detail */

template<typename T, typename Container, typename CharT>
    requires formattable<Container, CharT>
struct formatter<queue<T, Container>, CharT> {
private:
    formatter<Container, CharT> underlying_;

public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& context)
        -> typename ParseContext::iterator {
        return underlying_.parse(context);
    }

    template<typename Context>
    auto format(queue<T, Container>& value, Context& context) const {
        return underlying_.format(
            detail::format_container_access::get(value), context);
    }
};

template<typename T, typename Container, typename Compare, typename CharT>
    requires formattable<Container, CharT>
struct formatter<priority_queue<T, Container, Compare>, CharT> {
private:
    formatter<Container, CharT> underlying_;

public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& context)
        -> typename ParseContext::iterator {
        return underlying_.parse(context);
    }

    template<typename Context>
    auto format(priority_queue<T, Container, Compare>& value,
                Context& context) const {
        return underlying_.format(
            detail::format_container_access::get(value), context);
    }
};

template<typename T, typename Container, typename CharT>
    requires formattable<Container, CharT>
struct formatter<stack<T, Container>, CharT> {
private:
    formatter<Container, CharT> underlying_;

public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& context)
        -> typename ParseContext::iterator {
        return underlying_.parse(context);
    }

    template<typename Context>
    auto format(stack<T, Container>& value, Context& context) const {
        return underlying_.format(
            detail::format_container_access::get(value), context);
    }
};

} /* namespace std */

#endif /* C++23 */
#endif /* RINCXX_FORMAT_RANGES_H */
