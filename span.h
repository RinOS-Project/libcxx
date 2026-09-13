/*
 * RinOS C++ <span> ✿
 * スパン - C++20スタイルの非所有配列ビュー
 */

#ifndef RINCXX_SPAN_H
#define RINCXX_SPAN_H

#include "rincxx.h"

#if __cplusplus >= 202002L
#include "cstddef.h"
#include "type_traits.h"
#include "iterator.h"
#include "array.h"
#include "exception.h"
#include "initializer_list.h"
#include "memory.h"
#include "ranges.h"

namespace std {

/* Forward declaration for vector */
template<typename T, typename Allocator>
class vector;

/* ═══════════════════════════════════════════════════════════════
 * dynamic_extent
 * ═══════════════════════════════════════════════════════════════*/

inline constexpr size_t dynamic_extent = static_cast<size_t>(-1);

template<class T, size_t Extent = dynamic_extent>
class span;

namespace detail {

template<class T>
struct is_span_specialization : false_type {};

template<class T, size_t Extent>
struct is_span_specialization<span<T, Extent>> : true_type {};

[[noreturn]] inline void span_contract_fail() {
    /* Standard span contract violations cannot produce a usable view. */
    __builtin_trap();
}

/* C++26 specifies a recoverable diagnostic only for span::at().  Keep the
 * product's non-exception contract for constructors, operator[], and slicing:
 * those paths are noexcept or are precondition failures rather than checked
 * element access. */
[[noreturn]] inline void span_at_out_of_range() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("span::at position is out of range");
#else
    __builtin_trap();
#endif
}

template<class Value>
constexpr size_t span_range_size_value(Value value) noexcept
{
    if constexpr (is_signed<Value>::value) {
        if (value < 0) span_contract_fail();
    }
    return static_cast<size_t>(value);
}

template<class R>
constexpr size_t span_range_size(R&& range) noexcept
{
    auto first = ranges::begin(range);
    auto last = ranges::end(range);
    using iterator_type = decltype(first);
    using sentinel_type = decltype(last);
    if constexpr (sized_sentinel_for<sentinel_type, iterator_type>) {
        /* Do not route a negative pointer/sentinel distance through the
         * size_t-returning ranges::size CPO: that conversion would turn a
         * reversed range into a huge usable span. */
        return span_range_size_value(last - first);
    } else {
        return span_range_size_value(ranges::size(range));
    }
}

/* A span source must have the standard contiguous iterator shape, including
 * a correctly typed to_address result, before it can publish a raw view. */
template<class It, class Element, class = void>
struct span_iterator_compatible : false_type {};

template<class It, class Element>
struct span_iterator_compatible<It, Element, void_t<
    iter_reference_t<remove_cvref_t<It>>,
    decltype(std::to_address(declval<const remove_cvref_t<It>&>()))>>
    : integral_constant<bool,
        contiguous_iterator<remove_cvref_t<It>> &&
        is_convertible<
            remove_reference_t<iter_reference_t<remove_cvref_t<It>>>(*)[],
            Element(*)[]>::value &&
        is_convertible<
            decltype(std::to_address(declval<const remove_cvref_t<It>&>())),
            Element*>::value> {};

/* The range constructor uses range concepts rather than ad-hoc syntax, so
 * both sized opt-outs and malformed sized sentinels constrain every entry
 * point consistently. */
template<class R, class Element, class = void>
struct span_range_compatible : false_type {};

template<class R, class Element>
struct span_range_compatible<R, Element, void_t<
    decltype(ranges::begin(declval<R&>())),
    ranges::range_reference_t<R>,
    decltype(std::to_address(ranges::begin(declval<R&>())))>>
    : integral_constant<bool,
        ranges::contiguous_range<R> && ranges::sized_range<R> &&
        is_convertible<
            remove_reference_t<ranges::range_reference_t<R>>(*)[],
            Element(*)[]>::value &&
        is_convertible<
            decltype(std::to_address(ranges::begin(declval<R&>()))),
            Element*>::value> {};

template<class R>
constexpr auto span_range_address(R&& range) noexcept
    -> decltype(std::to_address(ranges::begin(declval<R&>())))
{
    return std::to_address(ranges::begin(range));
}

template<class T, size_t Extent>
struct span_storage {
    T* data_;

    constexpr span_storage(T* data, size_t count) noexcept : data_(data) {
        if (count != Extent || (count != 0 && data == nullptr)) {
            span_contract_fail();
        }
    }

    constexpr size_t size() const noexcept { return Extent; }
};

template<class T>
struct span_storage<T, dynamic_extent> {
    T* data_;
    size_t size_;

    constexpr span_storage(T* data, size_t count) noexcept
        : data_(data), size_(count) {
        if (count != 0 && data == nullptr) {
            span_contract_fail();
        }
    }

    constexpr size_t size() const noexcept { return size_; }
};

} /* namespace detail */

/* ═══════════════════════════════════════════════════════════════
 * span クラス
 * ═══════════════════════════════════════════════════════════════*/

template<class T, size_t Extent>
class span : private detail::span_storage<T, Extent> {
    using storage = detail::span_storage<T, Extent>;

    static_assert(is_object<T>::value,
                  "span element type must be an object type");
    static_assert(!is_abstract<T>::value,
                  "span element type cannot be abstract");

    constexpr T* offset_pointer(size_t offset) const noexcept {
        return offset == 0 ? this->data_ : this->data_ + offset;
    }

    static constexpr size_t pointer_count(T* first, T* last) noexcept {
        if (first == last) return 0;
        if (first == nullptr || last == nullptr || last < first) {
            detail::span_contract_fail();
        }
        return static_cast<size_t>(last - first);
    }

public:
    /* 型定義 */
    using element_type = T;
    using value_type = typename remove_cv<T>::type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    static constexpr size_type extent = Extent;
    
public:
    /* ═══════════════════════════════════════════════════════════
     * コンストラクタ
     * ═══════════════════════════════════════════════════════════*/
    
    /* デフォルト（dynamic_extentのみ） */
    template<size_t E = Extent,
             typename enable_if<E == dynamic_extent || E == 0, int>::type = 0>
    constexpr span() noexcept : storage(nullptr, 0) {}
    
    /* ポインタと長さ */
    constexpr explicit(Extent != dynamic_extent)
    span(pointer ptr, size_type count) noexcept
        : storage(ptr, count) {}

    /* General contiguous iterator + count.  The raw-pointer overload above
     * remains the ABI-simple fast path; this overload gives user-defined
     * contiguous iterators the same standard admission and extent checks. */
    template<class It,
             typename enable_if<
                 detail::span_iterator_compatible<It, element_type>::value,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(It first, size_type count) noexcept
        : storage(std::to_address(first), count) {}
    
    /* ポインタ範囲 */
    constexpr explicit(Extent != dynamic_extent)
    span(pointer first, pointer last) noexcept
        : storage(first, pointer_count(first, last)) {}

    /* A sentinel need not have the iterator's type.  Its distance must be
     * the iterator difference type, and a negative precondition violation is
     * rejected before a size_t extent can be published. */
    template<class It, class End,
             typename enable_if<
                 detail::span_iterator_compatible<It, element_type>::value &&
                 sized_sentinel_for<End, remove_cvref_t<It>>,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(It first, End last) noexcept
        : storage(std::to_address(first),
                  detail::span_range_size_value(last - first)) {}
    
    /* C配列 */
    template<size_t N,
             typename enable_if<Extent == dynamic_extent || Extent == N, int>::type = 0>
    constexpr span(element_type (&arr)[N]) noexcept
        : storage(arr, N) {}
    
    /* std::array */
    template<class U, size_t N,
             typename enable_if<
                 (Extent == dynamic_extent || Extent == N) &&
                 is_convertible<U(*)[], T(*)[]>::value,
                 int
             >::type = 0>
    constexpr span(array<U, N>& arr) noexcept
        : storage(arr.data(), N) {}
    
    template<class U, size_t N,
             typename enable_if<
                 (Extent == dynamic_extent || Extent == N) &&
                 is_convertible<const U(*)[], T(*)[]>::value,
                 int
             >::type = 0>
    constexpr span(const array<U, N>& arr) noexcept
        : storage(arr.data(), N) {}

    /* C++26 permits a read-only span over the compiler-owned backing array.
     * Mutable spans must not be made from an initializer list because its
     * elements are const and cannot provide writable storage. */
#if __cplusplus >= 202400L
    template<class U,
             typename enable_if<
                 is_const<element_type>::value &&
                 is_convertible<const U(*)[], element_type(*)[]>::value,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(initializer_list<U> values) noexcept
        : storage(values.begin(), values.size()) {}
#endif

    /* std::vectorからの変換 */
    template<class U, class Alloc,
             typename enable_if<
                 is_convertible<U(*)[], T(*)[]>::value,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(vector<U, Alloc>& v) noexcept
        : storage(v.data(), v.size()) {}

    template<class U, class Alloc,
             typename enable_if<
                 is_convertible<const U(*)[], T(*)[]>::value,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(const vector<U, Alloc>& v) noexcept
        : storage(v.data(), v.size()) {}

    /* 他のspanからの変換 */
    template<class U, size_t N,
             typename enable_if<
                 (Extent == dynamic_extent || N == dynamic_extent || Extent == N) &&
                 is_convertible<U(*)[], T(*)[]>::value,
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent && N == dynamic_extent)
    span(const span<U, N>& other) noexcept
        : storage(other.data(), other.size()) {}

    /* General C++20 contiguous/sized range.  An rvalue range can only make a
     * mutable view when it explicitly opts into borrowed_range; const-element
     * views retain the standard's read-only temporary-range allowance. */
    template<class R,
             typename enable_if<
                 detail::span_range_compatible<R, element_type>::value &&
                 !detail::is_span_specialization<
                     remove_cvref_t<R>>::value &&
                 (ranges::borrowed_range<R> || is_const<element_type>::value),
                 int
             >::type = 0>
    constexpr explicit(Extent != dynamic_extent)
    span(R&& range) noexcept
        : storage(detail::span_range_address(std::forward<R>(range)),
                  detail::span_range_size(std::forward<R>(range))) {}
    
    constexpr span(const span& other) noexcept = default;
    
    /* ═══════════════════════════════════════════════════════════
     * 代入
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr span& operator=(const span& other) noexcept = default;
    
    /* ═══════════════════════════════════════════════════════════
     * イテレータ
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr iterator begin() const noexcept { return this->data_; }
    constexpr iterator end() const noexcept { return offset_pointer(size()); }
    
    constexpr const_iterator cbegin() const noexcept { return this->data_; }
    constexpr const_iterator cend() const noexcept {
        return offset_pointer(size());
    }
    
    constexpr reverse_iterator rbegin() const noexcept { 
        return reverse_iterator(end()); 
    }
    constexpr reverse_iterator rend() const noexcept { 
        return reverse_iterator(begin()); 
    }
    
    constexpr const_reverse_iterator crbegin() const noexcept { 
        return const_reverse_iterator(cend()); 
    }
    constexpr const_reverse_iterator crend() const noexcept { 
        return const_reverse_iterator(cbegin()); 
    }
    
    /* ═══════════════════════════════════════════════════════════
     * 要素アクセス
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr reference front() const noexcept {
        if (empty()) detail::span_contract_fail();
        return this->data_[0];
    }
    constexpr reference back() const noexcept {
        if (empty()) detail::span_contract_fail();
        return this->data_[size() - 1];
    }
    
    constexpr reference operator[](size_type idx) const noexcept {
        if (idx >= size()) detail::span_contract_fail();
        return this->data_[idx];
    }

    /* C++26 checked access: exception-enabled hosted builds receive the
     * required out_of_range; the product no-unwind profile still stops before
     * it can manufacture an invalid reference. */
#if __cplusplus >= 202400L
    constexpr reference at(size_type idx) const {
        if (idx >= size()) detail::span_at_out_of_range();
        return this->data_[idx];
    }
#endif
    
    constexpr pointer data() const noexcept { return this->data_; }
    
    /* ═══════════════════════════════════════════════════════════
     * 容量
     * ═══════════════════════════════════════════════════════════*/
    
    constexpr size_type size() const noexcept { return storage::size(); }
    constexpr size_type size_bytes() const noexcept {
        if (size() > static_cast<size_type>(-1) / sizeof(T)) {
            detail::span_contract_fail();
        }
        return size() * sizeof(T);
    }
    constexpr bool empty() const noexcept { return size() == 0; }
    
    /* ═══════════════════════════════════════════════════════════
     * サブスパン
     * ═══════════════════════════════════════════════════════════*/
    
    template<size_t Count>
    constexpr span<element_type, Count> first() const noexcept {
        if constexpr (Extent != dynamic_extent) {
            static_assert(Count <= Extent, "span::first count exceeds extent");
        }
        if (Count > size()) detail::span_contract_fail();
        return span<element_type, Count>(this->data_, Count);
    }
    
    constexpr span<element_type, dynamic_extent> first(size_type count) const noexcept {
        if (count > size()) detail::span_contract_fail();
        return span<element_type, dynamic_extent>(this->data_, count);
    }
    
    template<size_t Count>
    constexpr span<element_type, Count> last() const noexcept {
        if constexpr (Extent != dynamic_extent) {
            static_assert(Count <= Extent, "span::last count exceeds extent");
        }
        if (Count > size()) detail::span_contract_fail();
        return span<element_type, Count>(offset_pointer(size() - Count), Count);
    }
    
    constexpr span<element_type, dynamic_extent> last(size_type count) const noexcept {
        if (count > size()) detail::span_contract_fail();
        return span<element_type, dynamic_extent>(
            offset_pointer(size() - count), count);
    }
    
    template<size_t Offset, size_t Count = dynamic_extent>
    constexpr auto subspan() const noexcept {
        if constexpr (Extent != dynamic_extent) {
            static_assert(Offset <= Extent,
                          "span::subspan offset exceeds extent");
            static_assert(Count == dynamic_extent || Count <= Extent - Offset,
                          "span::subspan count exceeds extent");
        }
        if (Offset > size()) detail::span_contract_fail();
        if constexpr (Count == dynamic_extent) {
            return span<element_type, 
                        Extent != dynamic_extent ? Extent - Offset : dynamic_extent>(
                offset_pointer(Offset), size() - Offset);
        } else {
            if (Count > size() - Offset) detail::span_contract_fail();
            return span<element_type, Count>(offset_pointer(Offset), Count);
        }
    }
    
    constexpr span<element_type, dynamic_extent> 
    subspan(size_type offset, size_type count = dynamic_extent) const noexcept {
        if (offset > size()) detail::span_contract_fail();
        const size_type remaining = size() - offset;
        if (count != dynamic_extent && count > remaining) {
            detail::span_contract_fail();
        }
        return span<element_type, dynamic_extent>(
            offset_pointer(offset),
            count == dynamic_extent ? remaining : count);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * 推論ガイド（C++17）
 * ═══════════════════════════════════════════════════════════════*/

template<class T, size_t N>
span(T (&)[N]) -> span<T, N>;

template<class T, size_t N>
span(array<T, N>&) -> span<T, N>;

template<class T, size_t N>
span(const array<T, N>&) -> span<const T, N>;

template<class It,
         typename enable_if<
             detail::span_iterator_compatible<
                 It, remove_reference_t<iter_reference_t<It>>>::value,
             int
         >::type = 0>
span(It, size_t) -> span<remove_reference_t<iter_reference_t<It>>>;

template<class It, class End,
         typename enable_if<
             detail::span_iterator_compatible<
                 It, remove_reference_t<iter_reference_t<It>>>::value &&
             sized_sentinel_for<End, remove_cvref_t<It>>,
             int
         >::type = 0>
span(It, End) -> span<remove_reference_t<iter_reference_t<It>>>;

template<class R,
         typename enable_if<
             detail::span_range_compatible<R,
                 typename remove_reference<decltype(
                     *ranges::begin(declval<R&>()))>::type>::value &&
             !detail::is_span_specialization<
                 remove_cvref_t<R>>::value,
             int
         >::type = 0>
span(R&&) -> span<typename remove_reference<
    ranges::range_reference_t<R>>::type>;

/* ═══════════════════════════════════════════════════════════════
 * as_bytes / as_writable_bytes
 * ═══════════════════════════════════════════════════════════════*/

template<class T, size_t N>
constexpr span<const byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N>
as_bytes(span<T, N> s) noexcept {
    return span<const byte,
                N == dynamic_extent ? dynamic_extent : sizeof(T) * N>(
        reinterpret_cast<const byte*>(s.data()), s.size_bytes());
}

template<class T, size_t N,
         typename enable_if<!is_const<T>::value, int>::type = 0>
constexpr span<byte, N == dynamic_extent ? dynamic_extent : sizeof(T) * N>
as_writable_bytes(span<T, N> s) noexcept {
    return span<byte,
                N == dynamic_extent ? dynamic_extent : sizeof(T) * N>(
        reinterpret_cast<byte*>(s.data()), s.size_bytes());
}

} /* namespace std */

#endif /* __cplusplus >= 202002L */

#endif /* RINCXX_SPAN_H */
