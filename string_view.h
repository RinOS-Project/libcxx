/*
 * RinOS C++ <string_view> header
 * Non-owning string reference (C++17)
 */

#ifndef RINCXX_STRING_VIEW_H
#define RINCXX_STRING_VIEW_H

#include "rincxx.h"

#if defined(__cplusplus) && __cplusplus >= 201703L
#include "cstddef.h"
#include "type_traits.h"
#include "algorithm.h"
#include "iterator.h"
#include "iosfwd.h"  /* For char_traits */
#include "__string_hash.h"
#include "exception.h"
#if __cplusplus >= 202002L
#include "compare.h"
#ifndef __cpp_lib_constexpr_string_view
#define __cpp_lib_constexpr_string_view 201811L
#endif
#endif

namespace std {

/* char_traits is defined in iosfwd.h */

template<typename CharT, typename Traits = char_traits<CharT>>
class basic_string_view {
public:
    using traits_type = Traits;
    using value_type = CharT;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using reference = CharT&;
    using const_reference = const CharT&;
    using const_iterator = const CharT*;
    using iterator = const_iterator;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    const_pointer m_data;
    size_type m_size;

    template<typename It, typename End>
    static constexpr size_type checked_iterator_distance(It first, End last) {
        /* Equality is well-defined for unrelated pointers and lets the two
         * null pointers represent an empty range without subtracting them. */
        if (first == last) return 0;
        if (!first || !last) {
            __detail::string_position_out_of_range(
                "basic_string_view iterator range contains null");
        }

        /* At runtime compare target-width addresses before forming a
         * difference.  This makes reversed and misaligned ranges
         * deterministic while avoiding undefined pointer subtraction for
         * unrelated allocations.  Constant evaluation uses the normal
         * same-array subtraction path, which remains constexpr. */
        if (!is_constant_evaluated()) {
            using address_type = __UINTPTR_TYPE__;
            const address_type first_address =
                reinterpret_cast<address_type>(first);
            const address_type last_address =
                reinterpret_cast<address_type>(last);
            if (first_address % alignof(CharT) != 0
                || last_address % alignof(CharT) != 0) {
                __detail::string_position_out_of_range(
                    "basic_string_view iterator range is misaligned");
            }
            if (last_address < first_address) {
                __detail::string_position_out_of_range(
                    "basic_string_view iterator range is reversed");
            }
            const address_type byte_distance = last_address - first_address;
            if (byte_distance % sizeof(CharT) != 0) {
                __detail::string_position_out_of_range(
                    "basic_string_view iterator range is misaligned");
            }
            const address_type distance = byte_distance / sizeof(CharT);
            if (distance > static_cast<address_type>(npos)) {
                __detail::string_position_out_of_range(
                    "basic_string_view iterator range is too large");
            }
            return static_cast<size_type>(distance);
        }

        const difference_type distance = last - first;
        if (distance < 0) {
            __detail::string_position_out_of_range(
                "basic_string_view iterator range is reversed");
        }
        return static_cast<size_type>(distance);
    }

#if __cplusplus >= 202002L
    template<typename It, typename End>
    static constexpr size_type checked_contiguous_distance(
        It first, End last, const_pointer data) {
        /* Do not ask a custom iterator to subtract a null value.  The
         * sentinel equality check is valid before any distance operation. */
        if (!data) {
            if (first == last) return 0;
            __detail::string_position_out_of_range(
                "basic_string_view contiguous range contains null");
        }

        const auto distance = last - first;
        if (distance < 0) {
            __detail::string_position_out_of_range(
                "basic_string_view contiguous range is reversed");
        }
        const size_type count = static_cast<size_type>(distance);
        if (static_cast<decltype(distance)>(count) != distance) {
            __detail::string_position_out_of_range(
                "basic_string_view contiguous range is too large");
        }
        if (!is_constant_evaluated()) {
            using address_type = __UINTPTR_TYPE__;
            const address_type data_address =
                reinterpret_cast<address_type>(data);
            if (data_address % alignof(CharT) != 0) {
                __detail::string_position_out_of_range(
                    "basic_string_view contiguous range is misaligned");
            }
        }
        return count;
    }

#if __cplusplus > 202002L
    template<typename R, typename = void>
    struct is_sized_data_range : false_type {};

    template<typename R>
    struct is_sized_data_range<R, void_t<
        decltype(declval<R&>().data()),
        decltype(declval<R&>().size())>>
        : integral_constant<bool,
            is_convertible<decltype(declval<R&>().data()),
                           const_pointer>::value &&
            is_convertible<decltype(declval<R&>().size()),
                           size_type>::value> {};

    template<typename R>
    static constexpr size_type checked_range_size(
        R& range, const_pointer data) {
        const auto raw_size = range.size();
        if constexpr (is_signed<decltype(raw_size)>::value) {
            if (raw_size < 0) {
                __detail::string_position_out_of_range(
                    "basic_string_view range size is negative");
            }
        }
        const size_type count = static_cast<size_type>(raw_size);
        if (static_cast<decltype(raw_size)>(count) != raw_size) {
            __detail::string_position_out_of_range(
                "basic_string_view range size is too large");
        }
        if (count != 0 && !data) {
            __detail::string_position_out_of_range(
                "basic_string_view range data is null");
        }
        if (!is_constant_evaluated() && data) {
            using address_type = __UINTPTR_TYPE__;
            const address_type data_address =
                reinterpret_cast<address_type>(data);
            if (data_address % alignof(CharT) != 0) {
                __detail::string_position_out_of_range(
                    "basic_string_view range data is misaligned");
            }
        }
        return count;
    }

    template<typename Pointer>
    struct is_const_data_pointer : false_type {};

    template<typename Pointee>
    struct is_const_data_pointer<const Pointee*> : true_type {};

    template<typename R, typename = void>
    struct is_const_data_range : false_type {};

    template<typename R>
    struct is_const_data_range<R, void_t<decltype(declval<R&>().data())>>
        : is_const_data_pointer<remove_cv_t<
              decltype(declval<R&>().data())>> {};

#endif
#endif

public:
    // Constructors
    constexpr basic_string_view() noexcept : m_data(nullptr), m_size(0) {}
    constexpr basic_string_view(const basic_string_view&) noexcept = default;
    constexpr basic_string_view(const CharT* s, size_type count)
        : m_data(s), m_size(count) {
        /* A non-empty view must have storage to address.  Keep the standard
         * empty/null representation valid, but reject the otherwise invalid
         * pair before any operation can dereference it. */
        if (!s && count != 0) {
            __detail::string_position_out_of_range(
                "basic_string_view constructor received null data");
        }
    }
    constexpr basic_string_view(const CharT* s) : m_data(s), m_size(s ? Traits::length(s) : 0) {}

    // C++20: Iterator constructor (for {begin, end} initialization)
    template<typename It, typename End,
             typename = typename enable_if<
                 is_same<typename remove_cv<typename remove_pointer<It>::type>::type, CharT>::value &&
                 is_same<typename remove_cv<typename remove_pointer<End>::type>::type, CharT>::value
             >::type>
    constexpr basic_string_view(It first, End last)
        : m_data(first), m_size(checked_iterator_distance(first, last)) {}

#if __cplusplus >= 202002L
    template<typename It, typename End>
        requires ((!is_pointer<It>::value || !is_pointer<End>::value) &&
                  contiguous_iterator<It> && sized_sentinel_for<End, It> &&
                  same_as<iter_value_t<It>, CharT> &&
                  same_as<remove_cvref_t<iter_reference_t<It>>, CharT>)
    constexpr basic_string_view(It first, End last)
        : m_data(to_address(first)),
          m_size(checked_contiguous_distance(first, last, to_address(first))) {}
#endif

#if __cplusplus > 202002L
    template<typename R>
        requires (is_sized_data_range<remove_reference_t<R>>::value &&
                  (is_lvalue_reference<R>::value ||
                   is_const_data_range<remove_reference_t<R>>::value))
    explicit constexpr basic_string_view(R&& range)
        : m_data(static_cast<const_pointer>(range.data())),
          m_size(checked_range_size(range, m_data)) {}
#endif

    // Note: Construction from std::string is handled via basic_string's conversion operator
    // to avoid ambiguity in ternary expressions. Do NOT add a templated constructor
    // from types with data()/size() as it causes conversion ambiguity.

    // Assignment
    constexpr basic_string_view& operator=(const basic_string_view&) noexcept = default;

    // Iterators
    constexpr const_iterator begin() const noexcept { return m_data; }
    constexpr const_iterator end() const noexcept {
        /* Pointer arithmetic on a null empty view is undefined even for a
         * zero offset.  Preserve the null representation instead. */
        return m_data ? m_data + m_size : nullptr;
    }
    constexpr const_iterator cbegin() const noexcept { return m_data; }
    constexpr const_iterator cend() const noexcept {
        return m_data ? m_data + m_size : nullptr;
    }

    // Reverse iterators
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // Element access
    constexpr const_reference operator[](size_type pos) const noexcept { return m_data[pos]; }
    constexpr const_reference at(size_type pos) const {
        if (pos >= m_size) {
            __detail::string_position_out_of_range(
                "basic_string_view::at position is out of range");
        }
        return m_data[pos];
    }
    constexpr const_reference front() const noexcept { return m_data[0]; }
    constexpr const_reference back() const noexcept { return m_data[m_size - 1]; }
    constexpr const_pointer data() const noexcept { return m_data; }

    // Capacity
    constexpr size_type size() const noexcept { return m_size; }
    constexpr size_type length() const noexcept { return m_size; }
    constexpr size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(CharT); }
    constexpr bool empty() const noexcept { return m_size == 0; }

    // Modifiers
    constexpr void remove_prefix(size_type n) noexcept {
        if (n > m_size) {
            __detail::string_position_out_of_range(
                "basic_string_view::remove_prefix exceeds size");
        }
        if (n != 0) {
            m_data += n;
        }
        m_size -= n;
    }

    constexpr void remove_suffix(size_type n) noexcept {
        if (n > m_size) {
            __detail::string_position_out_of_range(
                "basic_string_view::remove_suffix exceeds size");
        }
        m_size -= n;
    }

    constexpr void swap(basic_string_view& v) noexcept {
        auto tmp = *this;
        *this = v;
        v = tmp;
    }

    // Operations
    constexpr size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string_view::copy position is out of range");
        }
        size_type rlen = min(count, m_size - pos);
        if (rlen != 0) {
            if (!dest) {
                __detail::string_position_out_of_range(
                    "basic_string_view::copy destination is null");
            }
            Traits::copy(dest, m_data + pos, rlen);
        }
        return rlen;
    }

    constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string_view::substr position is out of range");
        }
        size_type rlen = min(count, m_size - pos);
        return basic_string_view(m_data ? m_data + pos : nullptr, rlen);
    }

    constexpr int compare(basic_string_view v) const noexcept {
        size_type rlen = min(m_size, v.m_size);
        int result = 0;
        if (rlen != 0) {
            result = Traits::compare(m_data, v.m_data, rlen);
        }
        if (result != 0) return result;
        if (m_size < v.m_size) return -1;
        if (m_size > v.m_size) return 1;
        return 0;
    }

    constexpr int compare(size_type pos1, size_type count1, basic_string_view v) const {
        return substr(pos1, count1).compare(v);
    }

    constexpr int compare(size_type pos1, size_type count1, basic_string_view v,
                         size_type pos2, size_type count2) const {
        return substr(pos1, count1).compare(v.substr(pos2, count2));
    }

    constexpr int compare(size_type pos1, size_type count1, const CharT* s) const {
        return substr(pos1, count1).compare(basic_string_view(s));
    }

    constexpr int compare(size_type pos1, size_type count1, const CharT* s,
                         size_type count2) const {
        return substr(pos1, count1).compare(basic_string_view(s, count2));
    }

    constexpr int compare(const CharT* s) const {
        return compare(basic_string_view(s));
    }

#if __cplusplus >= 202002L
    constexpr bool starts_with(basic_string_view sv) const noexcept {
        return m_size >= sv.m_size && compare(0, sv.m_size, sv) == 0;
    }

    constexpr bool starts_with(CharT c) const noexcept {
        return !empty() && Traits::eq(front(), c);
    }

    constexpr bool starts_with(const CharT* s) const {
        return starts_with(basic_string_view(s));
    }

    constexpr bool ends_with(basic_string_view sv) const noexcept {
        return m_size >= sv.m_size && compare(m_size - sv.m_size, npos, sv) == 0;
    }

    constexpr bool ends_with(CharT c) const noexcept {
        return !empty() && Traits::eq(back(), c);
    }

    constexpr bool ends_with(const CharT* s) const {
        return ends_with(basic_string_view(s));
    }

#endif

#if __cplusplus > 202002L
    constexpr bool contains(basic_string_view sv) const noexcept {
        return find(sv) != npos;
    }

    constexpr bool contains(CharT c) const noexcept {
        return find(c) != npos;
    }

    constexpr bool contains(const CharT* s) const {
        return find(s) != npos;
    }
#endif

    // Find operations
    constexpr size_type find(basic_string_view v, size_type pos = 0) const noexcept {
        if (v.empty()) return pos <= m_size ? pos : npos;
        if (pos >= m_size || v.m_size > m_size - pos) return npos;

        for (size_type i = pos; i <= m_size - v.m_size; ++i) {
            if (Traits::compare(m_data + i, v.m_data, v.m_size) == 0) {
                return i;
            }
        }
        return npos;
    }

    constexpr size_type find(CharT c, size_type pos = 0) const noexcept {
        if (pos >= m_size) return npos;
        const CharT* p = Traits::find(m_data + pos, m_size - pos, c);
        return p ? static_cast<size_type>(p - m_data) : npos;
    }

    constexpr size_type find(const CharT* s, size_type pos = 0) const {
        return find(basic_string_view(s), pos);
    }

    constexpr size_type rfind(basic_string_view v, size_type pos = npos) const noexcept {
        if (v.empty()) return min(pos, m_size);
        if (v.m_size > m_size) return npos;

        size_type last = min(pos, m_size - v.m_size);
        for (size_type i = last + 1; i > 0; --i) {
            if (Traits::compare(m_data + i - 1, v.m_data, v.m_size) == 0) {
                return i - 1;
            }
        }
        return npos;
    }

    constexpr size_type rfind(CharT c, size_type pos = npos) const noexcept {
        if (m_size == 0) return npos;
        size_type last = min(pos, m_size - 1);
        for (size_type i = last + 1; i > 0; --i) {
            if (Traits::eq(m_data[i - 1], c)) return i - 1;
        }
        return npos;
    }

    constexpr size_type find_first_of(basic_string_view v, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < m_size; ++i) {
            if (v.find(m_data[i]) != npos) return i;
        }
        return npos;
    }

    constexpr size_type find_first_of(CharT c, size_type pos = 0) const noexcept {
        return find(c, pos);
    }

    constexpr size_type find_last_of(basic_string_view v, size_type pos = npos) const noexcept {
        if (m_size == 0) return npos;
        size_type last = min(pos, m_size - 1);
        for (size_type i = last + 1; i > 0; --i) {
            if (v.find(m_data[i - 1]) != npos) return i - 1;
        }
        return npos;
    }

    constexpr size_type find_last_of(CharT c, size_type pos = npos) const noexcept {
        return rfind(c, pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos, size_type count) const noexcept {
        return find_last_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const noexcept {
        return find_last_of(basic_string_view(s), pos);
    }

    constexpr size_type find_first_not_of(basic_string_view v, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < m_size; ++i) {
            if (v.find(m_data[i]) == npos) return i;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept {
        for (size_type i = pos; i < m_size; ++i) {
            if (!Traits::eq(m_data[i], c)) return i;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const noexcept {
        return find_first_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos = 0) const noexcept {
        return find_first_not_of(basic_string_view(s), pos);
    }

    constexpr size_type find_last_not_of(basic_string_view v, size_type pos = npos) const noexcept {
        if (m_size == 0) return npos;
        size_type last = min(pos, m_size - 1);
        for (size_type i = last + 1; i > 0; --i) {
            if (v.find(m_data[i - 1]) == npos) return i - 1;
        }
        return npos;
    }

    constexpr size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept {
        if (m_size == 0) return npos;
        size_type last = min(pos, m_size - 1);
        for (size_type i = last + 1; i > 0; --i) {
            if (!Traits::eq(m_data[i - 1], c)) return i - 1;
        }
        return npos;
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const noexcept {
        return find_last_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos = npos) const noexcept {
        return find_last_not_of(basic_string_view(s), pos);
    }
};

// Comparison operators
template<typename CharT, typename Traits>
constexpr bool operator==(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template<typename CharT, typename Traits>
constexpr bool operator!=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return !(lhs == rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator<(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

template<typename CharT, typename Traits>
constexpr bool operator<=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return lhs.compare(rhs) <= 0;
}

template<typename CharT, typename Traits>
constexpr bool operator>(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return lhs.compare(rhs) > 0;
}

template<typename CharT, typename Traits>
constexpr bool operator>=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return lhs.compare(rhs) >= 0;
}

#if __cplusplus >= 202002L
template<typename CharT, typename Traits>
constexpr strong_ordering operator<=>(
    basic_string_view<CharT, Traits> lhs,
    basic_string_view<CharT, Traits> rhs) noexcept {
    const int result = lhs.compare(rhs);
    return result < 0 ? strong_ordering::less
         : result > 0 ? strong_ordering::greater
                      : strong_ordering::equal;
}

/* A string literal does not first convert to basic_string_view during
 * operator template deduction.  Keep the C-string spaceship overloads next
 * to the existing relational bridge so both operand orders participate in
 * the C++20 comparison category without allocating. */
template<typename CharT, typename Traits>
constexpr strong_ordering operator<=>(
    basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs <=> basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr strong_ordering operator<=>(
    const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) <=> rhs;
}
#endif

// Comparison with const CharT* (enables string_view == "literal")
template<typename CharT, typename Traits>
constexpr bool operator==(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs == basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator==(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) == rhs;
}

template<typename CharT, typename Traits>
constexpr bool operator!=(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return !(lhs == rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator!=(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return !(lhs == rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator<(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs < basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator<(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) < rhs;
}

template<typename CharT, typename Traits>
constexpr bool operator<=(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs <= basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator<=(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) <= rhs;
}

template<typename CharT, typename Traits>
constexpr bool operator>(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs > basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator>(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) > rhs;
}

template<typename CharT, typename Traits>
constexpr bool operator>=(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept {
    return lhs >= basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits>
constexpr bool operator>=(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept {
    return basic_string_view<CharT, Traits>(lhs) >= rhs;
}

// Type aliases
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
#if defined(__cpp_char8_t)
using u8string_view = basic_string_view<char8_t>;
#endif
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

template<typename T>
struct hash;

/* The standard specialization is for every basic_string_view character and
 * traits combination, not just the five aliases above.  Keeping this as a
 * partial specialization also makes a user-defined (but conforming) traits
 * type participate without requiring another explicit specialization. */
template<typename CharT, typename Traits>
struct hash<basic_string_view<CharT, Traits>> {
    size_t operator()(basic_string_view<CharT, Traits> view) const noexcept {
        return detail::libcxx_hash_character_sequence(view);
    }
};

// Literal operators
inline namespace literals {
inline namespace string_view_literals {
    constexpr string_view operator""_sv(const char* str, size_t len) noexcept {
        return string_view(str, len);
    }
    constexpr wstring_view operator""_sv(const wchar_t* str, size_t len) noexcept {
        return wstring_view(str, len);
    }
}
}

#if defined(RINCXX_STRING_H) && __cplusplus >= 202002L
template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::starts_with(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return sv.size() <= m_size &&
           Traits::compare(m_data, sv.data(), sv.size()) == 0;
}

template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::ends_with(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return sv.size() <= m_size &&
           Traits::compare(m_data + (m_size - sv.size()), sv.data(),
                           sv.size()) == 0;
}
#endif

#if defined(RINCXX_STRING_H) && __cplusplus >= 201703L
/* basic_string can only name basic_string_view after this class has been
 * completed: string_view itself reaches string.h through exception.h. */
template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>::basic_string(
    const StringViewLike& sv)
    : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
    const basic_string_view<CharT, Traits> view(sv);
    assign(view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>::basic_string(
    const StringViewLike& sv, const allocator_type& alloc)
    : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
    const basic_string_view<CharT, Traits> view(sv);
    assign(view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>::basic_string(
    const StringViewLike& sv, size_type pos, size_type count,
    const allocator_type& alloc)
    : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(pos, count);
    assign(part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::operator=(
    const StringViewLike& sv) {
    return assign(sv);
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::assign(
    const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return assign(view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::assign(
    const StringViewLike& sv, size_type pos, size_type count) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(pos, count);
    return assign(part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::operator+=(
    const StringViewLike& sv) {
    return append(sv);
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::append(
    const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return append(view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::append(
    const StringViewLike& sv, size_type pos, size_type count) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(pos, count);
    return append(part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::insert(
    size_type pos, const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return insert(pos, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::insert(
    size_type pos, const StringViewLike& sv, size_type subpos,
    size_type count) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(subpos, count);
    return insert(pos, part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::replace(
    size_type pos, size_type count, const StringViewLike& sv) {
    const basic_string_view<CharT, Traits> view(sv);
    return replace(pos, count, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::replace(
    size_type pos, size_type count, const StringViewLike& sv,
    size_type subpos, size_type count2) {
    const basic_string_view<CharT, Traits> view(sv);
    const basic_string_view<CharT, Traits> part = view.substr(subpos, count2);
    return replace(pos, count, part.data(), part.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             __detail::is_compatible_string_view<
                 StringViewLike, CharT, Traits>::value,
             int>::type>
basic_string<CharT, Traits, Allocator>&
basic_string<CharT, Traits, Allocator>::replace(
    const_iterator first, const_iterator last, const StringViewLike& sv) {
    return replace(static_cast<size_type>(first - m_data),
                   static_cast<size_type>(last - first), sv);
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::rfind(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return rfind(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find_first_of(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find_first_of(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find_last_of(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find_last_of(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find_first_not_of(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find_first_not_of(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
typename basic_string<CharT, Traits, Allocator>::size_type
basic_string<CharT, Traits, Allocator>::find_last_not_of(
    const StringViewLike& str, size_type pos) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return find_last_not_of(view.data(), pos, view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    const StringViewLike& str) const noexcept {
    const basic_string_view<CharT, Traits> view(str);
    return compare_data(m_data, m_size, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    size_type pos, size_type count, const StringViewLike& str) const {
    const basic_string_view<CharT, Traits> view(str);
    return compare(pos, count, view.data(), view.size());
}

template<typename CharT, typename Traits, typename Allocator>
template<typename StringViewLike,
         typename enable_if<
             is_convertible<const StringViewLike&,
                            basic_string_view<CharT, Traits>>::value &&
             !is_convertible<const StringViewLike&, const CharT*>::value,
             int>::type>
int basic_string<CharT, Traits, Allocator>::compare(
    size_type pos1, size_type count1, const StringViewLike& str,
    size_type pos2, size_type count2) const {
    const basic_string_view<CharT, Traits> view(str);
    const basic_string_view<CharT, Traits> part = view.substr(pos2, count2);
    return compare(pos1, count1, part.data(), part.size());
}
#endif

#if defined(RINCXX_STRING_H) && __cplusplus > 202002L
template<typename CharT, typename Traits, typename Allocator>
bool basic_string<CharT, Traits, Allocator>::contains(
    basic_string_view<CharT, Traits> sv) const noexcept {
    return find(sv.data(), 0, sv.size()) != npos;
}
#endif

} // namespace std

#endif /* C++17 string_view */
#endif /* RINCXX_STRING_VIEW_H */
