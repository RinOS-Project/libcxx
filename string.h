/*
 * RinOS C++ String ✿
 * std::string 互換実装
 */

#ifndef RINCXX_STRING_H
#define RINCXX_STRING_H

/* Include C string.h first for memcpy, strlen, etc. */
#include "../libc/string.h"

#include "rincxx.h"
#include "utility.h"  /* for std::declval */
#include "iosfwd.h"   /* for std::ostream forward declaration */
#include "iterator.h" /* for std::reverse_iterator */
#include "initializer_list.h"
#include "memory.h"   /* std::allocator and allocator_traits */
#include "__string_hash.h"
#include "../libc/internal/rin_float_parse.h"

#if __cplusplus >= 202002L
#include "compare.h"
#endif

#ifdef __cplusplus

namespace std {

#if __cplusplus > 202002L
#ifndef RINCXX_FROM_RANGE_T_DEFINED
#define RINCXX_FROM_RANGE_T_DEFINED
struct from_range_t {
    explicit constexpr from_range_t() = default;
};
inline constexpr from_range_t from_range{};
#endif
#endif

/* Keep dependent declarations parseable without making <string> include the
 * C++17 header while <string_view> is itself including <exception>. */
template<typename CharT, typename Traits> class basic_string_view;

/* Helper to detect string_view types - used to prevent ambiguous conversions */
namespace __detail {
    template<typename T> struct is_string_view_like : false_type {};
    template<typename C, typename Tr>
    struct is_string_view_like<basic_string_view<C, Tr>> : true_type {};

    template<typename T, typename CharT, typename Traits>
    struct is_compatible_string_view
        : integral_constant<bool,
              is_convertible<const T&,
                             basic_string_view<CharT, Traits>>::value &&
              !is_convertible<const T&, const CharT*>::value> {};

    [[noreturn]] inline void string_position_out_of_range(
        const char* diagnostic);
    [[noreturn]] inline void string_numeric_invalid(const char* diagnostic);
    [[noreturn]] inline void string_numeric_range(const char* diagnostic);
    inline bool string_allocation_failure();
    inline bool string_length_failure(const char* diagnostic);

    /* basic_string keeps a raw contiguous view for its character and iterator
     * surface, but allocation ownership must remain in the allocator's
     * pointer type.  Do not require arithmetic, subscript, or dereference on
     * that fancy pointer merely to address the owned character array. */
    template<typename T>
    constexpr T* string_pointer_address(T* pointer) noexcept {
        return pointer;
    }

#if __cplusplus >= 202002L
    /* C++20 allocators may expose a fancy pointer that deliberately omits
     * operator-> and provides only pointer_traits::to_address.  Keep the
     * raw contiguous view independent from pointer arithmetic while still
     * honoring that standard customization point. */
    template<typename Pointer>
    constexpr auto string_pointer_address(const Pointer& pointer) noexcept
        -> decltype(std::to_address(pointer)) {
        return std::to_address(pointer);
    }
#else
    template<typename Pointer>
    constexpr auto string_pointer_address(const Pointer& pointer) noexcept
        -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
        return pointer_traits<Pointer>::to_address(pointer);
    }

    template<typename Pointer>
    constexpr auto string_pointer_address(const Pointer& pointer) noexcept
        -> decltype(string_pointer_address(pointer.operator->())) {
        return string_pointer_address(pointer.operator->());
    }
#endif
}

/* ═══════════════════════════════════════════════════════════════
 * basic_string
 * 標準互換のため3パラメータテンプレートとする
 * TraitsとAllocatorを標準の所有規則に従って保持する。
 * ═══════════════════════════════════════════════════════════════*/

/* Historical spelling kept as an alias for source compatibility. */
template<typename T>
using default_string_allocator = allocator<T>;

template<typename CharT, typename Traits = char_traits<CharT>, typename Allocator = allocator<CharT>>
class basic_string {
public:
    using traits_type = Traits;
    using allocator_type = Allocator;
    using alloc_traits = allocator_traits<allocator_type>;
    using value_type = CharT;
    using size_type = size_t;
    using reference = CharT&;
    using const_reference = const CharT&;
    using pointer = typename alloc_traits::pointer;
    using const_pointer = typename alloc_traits::const_pointer;
    using iterator = CharT*;
    using const_iterator = const CharT*;
    
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
#if __cplusplus >= 201703L
    inline static CharT s_empty_storage[1] = {};
#else
    static CharT s_empty_storage[1];
#endif

    static int compare_data(const CharT* left, size_type left_count,
                            const CharT* right, size_type right_count) noexcept {
        const size_type common = left_count < right_count ? left_count : right_count;
        if (common != 0) {
            const int comparison = Traits::compare(left, right, common);
            if (comparison != 0) return comparison;
        }
        if (left_count < right_count) return -1;
        if (left_count > right_count) return 1;
        return 0;
    }

    /* m_allocation is deliberately distinct from m_data.  The latter is the
     * raw contiguous character surface required by data()/iterators; the
     * former is the exact fancy pointer which must return storage to its
     * originating allocator. */
    pointer m_allocation = pointer();
    CharT* m_data;
    size_type m_size;
    size_type m_capacity;
    allocator_type m_alloc;
    
    static constexpr size_type MIN_CAPACITY = 16;

    size_type maximum_size_value() const noexcept {
        const size_type representation_limit =
            static_cast<size_type>(-1) / sizeof(CharT) - 1;
        const size_type allocation_count =
            static_cast<size_type>(alloc_traits::max_size(m_alloc));
        if (allocation_count == 0) {
            return 0;
        }
        const size_type allocation_limit = allocation_count - 1;
        return allocation_limit < representation_limit
            ? allocation_limit : representation_limit;
    }

    bool checked_growth(size_type base, size_type extra,
                        size_type& result,
                        const char* diagnostic) {
        const size_type limit = maximum_size_value();
        if (base > limit || extra > limit - base) {
            return __detail::string_length_failure(diagnostic);
        }
        result = base + extra;
        return true;
    }

    static CharT* empty_sentinel() noexcept {
        return s_empty_storage;
    }

    bool uses_heap_storage() const noexcept {
        return m_data && m_data != empty_sentinel();
    }

    bool source_aliases_current_storage(const CharT* source, size_type count, size_type& offset) const noexcept {
        if (!source || !uses_heap_storage())
            return false;

        using address_type = __UINTPTR_TYPE__;
        const address_type begin_address = reinterpret_cast<address_type>(m_data);
        const address_type source_address = reinterpret_cast<address_type>(source);
        if (source_address < begin_address)
            return false;

        const address_type byte_offset = source_address - begin_address;
        if (byte_offset % sizeof(CharT) != 0
            || byte_offset / sizeof(CharT) > m_size)
            return false;

        offset = static_cast<size_type>(byte_offset / sizeof(CharT));
        if (count > m_size - offset)
            return false;
        return true;
    }

    void reset_empty() noexcept {
        m_allocation = pointer();
        m_data = empty_sentinel();
        m_size = 0;
        m_capacity = 0;
    }

    void release_storage() noexcept {
        if (uses_heap_storage()) {
            alloc_traits::deallocate(m_alloc, m_allocation, m_capacity + 1);
        }
        reset_empty();
    }

    bool ensure_capacity(size_type required) {
        if (!m_data) {
            reset_empty();
        }
        if (required == 0) {
            return true;
        }
        const size_type limit = maximum_size_value();
        if (required > limit) {
            return __detail::string_length_failure(
                "basic_string capacity exceeds max_size");
        }
        if (required <= m_capacity && m_data != empty_sentinel()) {
            return true;
        }

        size_type cap = required < MIN_CAPACITY ? MIN_CAPACITY : required;
        if (m_capacity >= MIN_CAPACITY
            && m_capacity <= limit / 2) {
            size_type doubled = m_capacity * 2;
            if (doubled > cap) cap = doubled;
        }

        if (cap > limit) {
            cap = limit;
        }
#ifdef RIN_DEBUG_STL
        size_type alloc_size = (cap + 1) * sizeof(CharT);
        if (alloc_size > 0x100000) {
            rin_log("[STR_GROW] large allocation\n");
        }
#endif
        pointer new_allocation = alloc_traits::allocate(m_alloc, cap + 1);
        CharT* new_data = __detail::string_pointer_address(new_allocation);
        if (!new_data) {
            /* Allocation ownership is established before the raw contiguous
             * view is derived.  If a fancy pointer advertises no address,
            * return that exact owner immediately; otherwise every failed
            * growth leaks the candidate block. */
            alloc_traits::deallocate(m_alloc, new_allocation, cap + 1);
            return __detail::string_allocation_failure();
        }

        const CharT* old_data = m_data;
        size_type copy_len = m_size;
        if (copy_len > required) {
            copy_len = required;
        }
        for (size_type i = 0; i < copy_len; i++) {
            new_data[i] = old_data[i];
        }
        new_data[copy_len] = CharT(0);

        if (old_data && old_data != empty_sentinel()) {
            alloc_traits::deallocate(m_alloc, m_allocation,
                                     m_capacity + 1);
        }
        m_allocation = new_allocation;
        m_data = new_data;
        m_capacity = cap;
        if (m_size > copy_len) {
            m_size = copy_len;
        }
        return true;
    }
    
    static size_type strlen(const CharT* s) {
        if (!s) {
            __detail::string_position_out_of_range(
                "basic_string null C-string source");
        }
        size_type len = 0;
        while (s[len]) len++;
        return len;
    }

    /* Pointer/count overloads do not scan for a terminator, but a non-empty
     * range still needs a real source before any allocation or mutation.  A
     * null pointer is accepted only for the zero-length range, whose loops
     * and traits operations never dereference it. */
    static void validate_pointer_count(const CharT* source,
                                       size_type count) {
        if (count != 0 && !source) {
            __detail::string_position_out_of_range(
                "basic_string null pointer with nonzero count");
        }
    }

    /* Iterator-taking modifiers must validate that both iterators belong to
     * this string before doing pointer subtraction.  Subtracting pointers
     * from different allocations is itself undefined, so compare their
     * target-width addresses and only form a difference after the range and
     * element alignment have been established. */
    size_type iterator_index(const_iterator position) const {
        if (!position) {
            __detail::string_position_out_of_range(
                "basic_string iterator is null");
        }
        using address_type = __UINTPTR_TYPE__;
        const address_type begin_address =
            reinterpret_cast<address_type>(m_data);
        const address_type position_address =
            reinterpret_cast<address_type>(position);
        if (position_address < begin_address) {
            __detail::string_position_out_of_range(
                "basic_string iterator does not belong to string");
        }
        const address_type byte_offset = position_address - begin_address;
        if (byte_offset % sizeof(CharT) != 0) {
            __detail::string_position_out_of_range(
                "basic_string iterator is misaligned");
        }
        const address_type index_address = byte_offset / sizeof(CharT);
        if (index_address > static_cast<address_type>(m_size)) {
            __detail::string_position_out_of_range(
                "basic_string iterator is outside string");
        }
        return static_cast<size_type>(index_address);
    }

    void iterator_range_indices(const_iterator first, const_iterator last,
                                size_type& position, size_type& count) const {
        position = iterator_index(first);
        const size_type end = iterator_index(last);
        if (end < position) {
            __detail::string_position_out_of_range(
                "basic_string iterator range is reversed");
        }
        count = end - position;
    }

public:
    /* コンストラクタ */
    basic_string()
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {}

    explicit basic_string(const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {}
    
    basic_string(const CharT* s)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        size_type len = strlen(s);
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        for (size_type i = 0; i <= len; i++) {
            m_data[i] = s[i];
        }
    }

    basic_string(const CharT* s, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        size_type len = strlen(s);
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        for (size_type i = 0; i <= len; i++) {
            m_data[i] = s[i];
        }
    }
    
    basic_string(const CharT* s, size_type count)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        validate_pointer_count(s, count);
        if (count == 0) return;
        if (!ensure_capacity(count)) return;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = s[i];
        }
        m_data[count] = CharT(0);
    }

    basic_string(const CharT* s, size_type count, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        validate_pointer_count(s, count);
        if (count == 0) return;
        if (!ensure_capacity(count)) return;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = s[i];
        }
        m_data[count] = CharT(0);
    }
    
    basic_string(size_type count, CharT ch)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        if (count == 0) return;
        if (!ensure_capacity(count)) return;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = ch;
        }
        m_data[count] = CharT(0);
    }

    basic_string(size_type count, CharT ch, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        if (count == 0) return;
        if (!ensure_capacity(count)) return;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = ch;
        }
        m_data[count] = CharT(0);
    }

    basic_string(initializer_list<CharT> values,
                 const allocator_type& alloc = allocator_type())
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        const size_type count = values.size();
        if (count == 0 || !ensure_capacity(count)) return;
        const CharT* source = values.begin();
        for (size_type i = 0; i < count; i++) {
            m_data[i] = source[i];
        }
        m_size = count;
        m_data[m_size] = CharT(0);
    }

#if __cplusplus > 202002L
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    basic_string(from_range_t, Range&& range,
                 const allocator_type& alloc = allocator_type())
        : basic_string(alloc) {
        assign_range(std::forward<Range>(range));
    }
#endif
    
    basic_string(const basic_string& other)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0),
          m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)) {
        /* Optional copy diagnostics for unusually large strings */
#ifdef RIN_DEBUG_STL
        if (other.m_size > 0x10000) {
            rin_log("[STR_COPY] large source string\n");
        }
#endif
        if (other.m_size == 0) return;
        if (!ensure_capacity(other.m_size)) return;
        m_size = other.m_size;
        for (size_type i = 0; i <= m_size; i++) {
            m_data[i] = other.m_data[i];
        }
    }

    basic_string(const basic_string& other, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        if (other.m_size == 0) return;
        if (!ensure_capacity(other.m_size)) return;
        m_size = other.m_size;
        for (size_type i = 0; i <= m_size; i++) {
            m_data[i] = other.m_data[i];
        }
    }

    // 部分文字列コンストラクタ (pos, count)
    basic_string(const basic_string& other, size_type pos, size_type count = npos)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0),
          m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)) {
        if (pos > other.m_size) {
            __detail::string_position_out_of_range(
                "basic_string substring constructor position is out of range");
        }
        size_type available = other.m_size - pos;
        size_type actual = (count == npos || count > available)
                          ? available : count;
        if (actual == 0) return;
        if (!ensure_capacity(actual)) return;
        m_size = actual;
        for (size_type i = 0; i < actual; i++) {
            m_data[i] = other.m_data[pos + i];
        }
        m_data[actual] = CharT(0);
    }

    basic_string(const basic_string& other, size_type pos,
                 const allocator_type& alloc)
        : basic_string(other, pos, npos, alloc) {}

    basic_string(const basic_string& other, size_type pos, size_type count,
                 const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        if (pos > other.m_size) {
            __detail::string_position_out_of_range(
                "basic_string substring constructor position is out of range");
        }
        size_type available = other.m_size - pos;
        size_type actual = (count == npos || count > available)
                          ? available : count;
        if (actual == 0) return;
        if (!ensure_capacity(actual)) return;
        m_size = actual;
        for (size_type i = 0; i < actual; i++) {
            m_data[i] = other.m_data[pos + i];
        }
        m_data[actual] = CharT(0);
    }

    basic_string(basic_string&& other) noexcept(is_nothrow_move_constructible<allocator_type>::value)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0),
          m_alloc(std::move(other.m_alloc)) {
        if (other.m_data && other.m_data != empty_sentinel()) {
            m_allocation = other.m_allocation;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
        }
        other.reset_empty();
    }

    basic_string(basic_string&& other, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        if (m_alloc == other.m_alloc) {
            if (other.m_data && other.m_data != empty_sentinel()) {
                m_allocation = other.m_allocation;
                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
            }
            other.reset_empty();
            return;
        }
        if (other.m_size == 0) {
            return;
        }
        append(other.m_data, other.m_size);
        if (m_size == other.m_size) {
            other.clear();
        }
    }

    // Explicit constructor from string_view (to prevent ternary ambiguity)
    explicit basic_string(basic_string_view<CharT, char_traits<CharT>> sv)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        size_type len = static_cast<size_type>(sv.size());
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        const CharT* src = sv.data();
        for (size_type i = 0; i < len; i++) {
            m_data[i] = src[i];
        }
        m_data[len] = CharT(0);
    }

    explicit basic_string(basic_string_view<CharT, char_traits<CharT>> sv,
                          const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        size_type len = static_cast<size_type>(sv.size());
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        const CharT* src = sv.data();
        for (size_type i = 0; i < len; i++) {
            m_data[i] = src[i];
        }
        m_data[len] = CharT(0);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    explicit basic_string(const StringViewLike& sv);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    explicit basic_string(const StringViewLike& sv, const allocator_type& alloc);

    /* The slice constructor is deliberately also available for a C-string:
     * its extra position/count arguments make this a distinct operation. */
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value,
                 int>::type = 0>
    basic_string(const StringViewLike& sv, size_type pos, size_type count,
                 const allocator_type& alloc = allocator_type());
#endif

    // Constructor from other types with data() and size() (excluding string_view)
    template<typename T,
             typename = typename enable_if<
                 !__detail::is_string_view_like<typename remove_cv<typename remove_reference<T>::type>::type>::value &&
#if __cplusplus >= 201703L
                 !__detail::is_compatible_string_view<T, CharT, Traits>::value &&
#endif
                 !is_same<typename decay<T>::type, basic_string>::value
             >::type,
             typename = decltype(static_cast<const CharT*>(declval<const T&>().data())),
             typename = decltype(static_cast<size_type>(declval<const T&>().size()))>
    basic_string(const T& sv)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        size_type len = static_cast<size_type>(sv.size());
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        const CharT* src = sv.data();
        for (size_type i = 0; i < len; i++) {
            m_data[i] = src[i];
        }
        m_data[len] = CharT(0);
    }

    template<typename T,
             typename = typename enable_if<
                 !__detail::is_string_view_like<typename remove_cv<typename remove_reference<T>::type>::type>::value &&
#if __cplusplus >= 201703L
                 !__detail::is_compatible_string_view<T, CharT, Traits>::value &&
#endif
                 !is_same<typename decay<T>::type, basic_string>::value
             >::type,
             typename = decltype(static_cast<const CharT*>(declval<const T&>().data())),
             typename = decltype(static_cast<size_type>(declval<const T&>().size()))>
    basic_string(const T& sv, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        size_type len = static_cast<size_type>(sv.size());
        if (len == 0) return;
        if (!ensure_capacity(len)) return;
        m_size = len;
        const CharT* src = sv.data();
        for (size_type i = 0; i < len; i++) {
            m_data[i] = src[i];
        }
        m_data[len] = CharT(0);
    }

    // Iterator constructor (InputIterator first, InputIterator last)
    template<typename InputIt,
             typename = decltype(*declval<InputIt&>()),
             typename = decltype(++declval<InputIt&>())>
    basic_string(InputIt first, InputIt last)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc() {
        basic_string staged(m_alloc);
        for (; first != last; ++first) {
            size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*first));
            if (staged.size() == old_size) {
                return;
            }
        }
        swap(staged);
    }

    template<typename InputIt,
             typename = decltype(*declval<InputIt&>()),
             typename = decltype(++declval<InputIt&>())>
    basic_string(InputIt first, InputIt last, const allocator_type& alloc)
        : m_data(empty_sentinel()), m_size(0), m_capacity(0), m_alloc(alloc) {
        basic_string staged(m_alloc);
        for (; first != last; ++first) {
            size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*first));
            if (staged.size() == old_size) {
                return;
            }
        }
        swap(staged);
    }

    ~basic_string() {
        release_storage();
    }

    /* Implicit conversion to string_view */
    operator basic_string_view<CharT, Traits>() const noexcept {
        return basic_string_view<CharT, Traits>(m_data, m_size);
    }

    /* 代入 */
    basic_string& operator=(const basic_string& other) {
        if (this != &other) {
            if (alloc_traits::propagate_on_container_copy_assignment::value
                && m_alloc != other.m_alloc) {
                release_storage();
                m_alloc = other.m_alloc;
            }
            if (other.m_size == 0) {
                clear();
                return *this;
            }
            if (!ensure_capacity(other.m_size)) return *this;
            m_size = other.m_size;
            for (size_type i = 0; i <= m_size; i++) {
                m_data[i] = other.m_data[i];
            }
        }
        return *this;
    }
    
    basic_string& operator=(basic_string&& other) noexcept(
        alloc_traits::propagate_on_container_move_assignment::value ||
        alloc_traits::is_always_equal::value) {
        if (this != &other) {
            const bool propagate =
                alloc_traits::propagate_on_container_move_assignment::value;
            const bool can_steal = propagate
                || alloc_traits::is_always_equal::value
                || m_alloc == other.m_alloc;
            if (can_steal) {
                release_storage();
                if (propagate) {
                    m_alloc = std::move(other.m_alloc);
                }
                if (other.m_data && other.m_data != empty_sentinel()) {
                    m_allocation = other.m_allocation;
                    m_data = other.m_data;
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                }
                other.reset_empty();
            } else {
                if (other.m_size == 0) {
                    clear();
                } else {
                    basic_string staged(other, m_alloc);
                    if (staged.m_size == other.m_size) {
                        swap(staged);
                        other.clear();
                    }
                }
            }
        }
        return *this;
    }
    
    basic_string& operator=(const CharT* s) {
        size_type len = strlen(s);
        size_type source_offset = 0;
        bool source_aliases_storage = source_aliases_current_storage(s, len, source_offset);
        if (len == 0) {
            clear();
            return *this;
        }
        if (!ensure_capacity(len)) return *this;
        if (source_aliases_storage)
            s = m_data + source_offset;
        m_size = len;
        for (size_type i = 0; i <= len; i++) {
            m_data[i] = s[i];
        }
        return *this;
    }

    basic_string& operator=(CharT ch) {
        return assign(static_cast<size_type>(1), ch);
    }

    basic_string& operator=(initializer_list<CharT> values) {
        return assign(values.begin(), values.end());
    }

    /* Assignment from string_view */
    template<typename Traits2 = char_traits<CharT>>
    basic_string& operator=(basic_string_view<CharT, Traits2> sv) {
        size_type len = sv.size();
        const CharT* source = sv.data();
        size_type source_offset = 0;
        bool source_aliases_storage = source_aliases_current_storage(source, len, source_offset);
        if (len == 0) {
            clear();
            return *this;
        }
        if (!ensure_capacity(len)) return *this;
        if (source_aliases_storage)
            source = m_data + source_offset;
        m_size = len;
        for (size_type i = 0; i < len; i++) {
            m_data[i] = source[i];
        }
        m_data[len] = CharT();
        return *this;
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& operator=(const StringViewLike& sv);
#endif

    /* assign */
    basic_string& assign(size_type count, CharT ch) {
        if (count == 0) {
            clear();
            return *this;
        }
        if (!ensure_capacity(count)) return *this;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = ch;
        }
        m_data[count] = CharT(0);
        return *this;
    }

    basic_string& assign(const basic_string& str) {
        return *this = str;
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& assign(const StringViewLike& sv);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& assign(const StringViewLike& sv, size_type pos,
                         size_type count = npos);
#endif

    basic_string& assign(const basic_string& str, size_type pos, size_type count = npos) {
        if (pos > str.m_size) {
            __detail::string_position_out_of_range(
                "basic_string::assign position is out of range");
        }
        size_type available = str.m_size - pos;
        size_type actual = (count == npos || count > available)
                          ? available : count;
        const CharT* source = str.m_data + pos;
        size_type source_offset = 0;
        bool source_aliases_storage = (&str == this)
            && source_aliases_current_storage(source, actual, source_offset);
        if (actual == 0) {
            clear();
            return *this;
        }
        if (!ensure_capacity(actual)) return *this;
        if (source_aliases_storage)
            source = m_data + source_offset;
        m_size = actual;
        for (size_type i = 0; i < actual; i++) {
            m_data[i] = source[i];
        }
        m_data[actual] = CharT(0);
        return *this;
    }

    basic_string& assign(const CharT* s, size_type count) {
        validate_pointer_count(s, count);
        size_type source_offset = 0;
        bool source_aliases_storage = source_aliases_current_storage(s, count, source_offset);
        if (count == 0) {
            clear();
            return *this;
        }
        if (!ensure_capacity(count)) return *this;
        if (source_aliases_storage)
            s = m_data + source_offset;
        m_size = count;
        for (size_type i = 0; i < count; i++) {
            m_data[i] = s[i];
        }
        m_data[count] = CharT(0);
        return *this;
    }

    basic_string& assign(const CharT* s) {
        return *this = s;
    }

    basic_string& assign(initializer_list<CharT> values) {
        return assign(values.begin(), values.end());
    }

    basic_string& assign(basic_string&& str) noexcept {
        return *this = std::move(str);
    }

    template<typename InputIt>
    basic_string& assign(InputIt first, InputIt last) {
        basic_string staged(m_alloc);
        for (; first != last; ++first) {
            size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*first));
            if (staged.size() == old_size) {
                return *this;
            }
        }
        swap(staged);
        return *this;
    }

#if __cplusplus > 202002L
    /* C++23 range modifiers deliberately route through the existing iterator
     * implementations.  Those paths stage input before publishing, which
     * keeps self-ranges and allocation failure from exposing a partial
     * destination.  The default template arguments make non-ranges fail by
     * substitution instead of producing an unconstrained overload. */
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    basic_string& assign_range(Range&& range) {
        return assign(::std::begin(range), ::std::end(range));
    }
#endif

    /* アクセス */
    reference operator[](size_type pos) noexcept { return m_data[pos]; }
    const_reference operator[](size_type pos) const noexcept { return m_data[pos]; }
    
    reference at(size_type pos) {
        if (pos >= m_size) {
            __detail::string_position_out_of_range(
                "basic_string::at position is out of range");
        }
        return m_data[pos];
    }
    const_reference at(size_type pos) const {
        if (pos >= m_size) {
            __detail::string_position_out_of_range(
                "basic_string::at position is out of range");
        }
        return m_data[pos];
    }
    
    reference front() noexcept { return m_data[0]; }
    const_reference front() const noexcept { return m_data[0]; }
    
    reference back() noexcept { return m_data[m_size - 1]; }
    const_reference back() const noexcept { return m_data[m_size - 1]; }
    
    /* data()/c_str() are contiguous character views, not allocator handles.
     * Keep their standard raw-pointer contract even when allocator::pointer
     * is a fancy owner. */
    CharT* data() noexcept { return m_data; }
    const CharT* data() const noexcept { return m_data; }
    const CharT* c_str() const noexcept { return m_data; }
    
    /* イテレータ */
    iterator begin() noexcept { return m_data; }
    const_iterator begin() const noexcept { return m_data; }
    iterator end() noexcept { return m_data + m_size; }
    const_iterator end() const noexcept { return m_data + m_size; }
    const_iterator cbegin() const noexcept { return m_data; }
    const_iterator cend() const noexcept { return m_data + m_size; }

    /* リバースイテレータ */
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    /* 容量 */
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type length() const noexcept { return m_size; }
    size_type capacity() const noexcept { return m_capacity; }
    size_type max_size() const noexcept {
        /* Maximum possible string size (leave room for null terminator) */
        return maximum_size_value();
    }

    allocator_type get_allocator() const noexcept { return m_alloc; }

    void reserve(size_type new_cap) {
        ensure_capacity(new_cap);
    }

#if __cplusplus >= 202002L
    /* C++20's no-argument reserve is the non-binding shrink request.  Keep
     * the legacy reserve(size_type) growth contract separate so callers do
     * not accidentally turn a zero argument into a large request. */
    void reserve() {
        shrink_to_fit();
    }
#endif

    /* C++11 shrink_to_fit is a non-binding request.  When the allocator can
     * provide the exact current extent, complete that replacement before
     * releasing the old allocation so failure leaves this string unchanged. */
    void shrink_to_fit() {
        if (!uses_heap_storage() || m_capacity == m_size) return;

        if (m_size == 0) {
            release_storage();
            return;
        }

        const size_type new_capacity = m_size;
        pointer new_allocation =
            alloc_traits::allocate(m_alloc, new_capacity + 1);
        CharT* new_data = __detail::string_pointer_address(new_allocation);
        if (!new_data) {
            /* Keep shrink_to_fit failure-atomic and release the candidate
             * allocation whose address conversion failed. */
            alloc_traits::deallocate(m_alloc, new_allocation, new_capacity + 1);
            (void)__detail::string_allocation_failure();
            return;
        }
        for (size_type index = 0; index <= m_size; ++index)
            new_data[index] = m_data[index];

        alloc_traits::deallocate(m_alloc, m_allocation, m_capacity + 1);
        m_allocation = new_allocation;
        m_data = new_data;
        m_capacity = new_capacity;
    }
    
    void clear() noexcept {
        /* RinOS keeps the canonical empty representation observable across
         * translation units, so clearing an owned buffer returns it through
         * the same allocator instead of retaining a private empty buffer. */
        release_storage();
    }

    void swap(basic_string& other) noexcept(
        alloc_traits::propagate_on_container_swap::value ||
        alloc_traits::is_always_equal::value) {
        if (alloc_traits::propagate_on_container_swap::value) {
            using std::swap;
            swap(m_alloc, other.m_alloc);
        } else if (!alloc_traits::is_always_equal::value &&
                   !(m_alloc == other.m_alloc)) {
            /* Non-propagating unequal allocators must never exchange storage:
             * the eventual deallocator is part of the allocation owner.  Build
             * both replacement strings with their destination allocators
             * before touching either object, so allocation/length failure is
             * failure-atomic and the old strings retain their buffers. */
            if (other.m_size > maximum_size_value()) {
                (void)__detail::string_length_failure(
                    "basic_string::swap exceeds max_size");
                return;
            }
            if (m_size > other.maximum_size_value()) {
                (void)__detail::string_length_failure(
                    "basic_string::swap exceeds max_size");
                return;
            }
            basic_string replacement_for_this(other.m_data, other.m_size,
                                              m_alloc);
            basic_string replacement_for_other(m_data, m_size, other.m_alloc);

            pointer this_allocation = replacement_for_this.m_allocation;
            CharT* this_data = replacement_for_this.m_data;
            size_type this_size = replacement_for_this.m_size;
            size_type this_capacity = replacement_for_this.m_capacity;
            pointer other_allocation = replacement_for_other.m_allocation;
            CharT* other_data = replacement_for_other.m_data;
            size_type other_size = replacement_for_other.m_size;
            size_type other_capacity = replacement_for_other.m_capacity;

            replacement_for_this.reset_empty();
            replacement_for_other.reset_empty();
            release_storage();
            other.release_storage();
            m_allocation = this_allocation;
            m_data = this_data;
            m_size = this_size;
            m_capacity = this_capacity;
            other.m_allocation = other_allocation;
            other.m_data = other_data;
            other.m_size = other_size;
            other.m_capacity = other_capacity;
            return;
        }

        /* Propagating or equal allocators may exchange their exact allocation
         * handles; each side can still return the storage through an
         * equivalent owner. */
        pointer tmp_allocation = m_allocation;
        CharT* tmp_data = m_data;
        size_type tmp_size = m_size;
        size_type tmp_cap = m_capacity;
        m_allocation = other.m_allocation;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_allocation = tmp_allocation;
        other.m_data = tmp_data;
        other.m_size = tmp_size;
        other.m_capacity = tmp_cap;
    }

    /* 変更 */
    basic_string& operator+=(const basic_string& str) {
        return append(str);
    }
    
    basic_string& operator+=(const CharT* s) {
        return append(s);
    }
    
    basic_string& operator+=(CharT ch) {
        push_back(ch);
        return *this;
    }

    basic_string& operator+=(initializer_list<CharT> values) {
        return append(values.begin(), values.end());
    }

    basic_string& operator+=(basic_string_view<CharT, Traits> sv) {
        return append(sv.data(), sv.size());
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& operator+=(const StringViewLike& sv);
#endif

    basic_string& append(const basic_string& str) {
        return append(str.m_data, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& append(const StringViewLike& sv);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& append(const StringViewLike& sv, size_type pos,
                         size_type count = npos);
#endif

    basic_string& append(const basic_string& str, size_type pos, size_type count = npos) {
        if (pos > str.m_size) {
            __detail::string_position_out_of_range(
                "basic_string::append position is out of range");
        }
        size_type available = str.m_size - pos;
        size_type rcount = (count == npos || count > available)
                         ? available : count;
        return append(str.m_data + pos, rcount);
    }

    basic_string& append(const CharT* s) {
        return append(s, strlen(s));
    }
    
    basic_string& append(const CharT* s, size_type count) {
        validate_pointer_count(s, count);
        size_type source_offset = 0;
        bool source_aliases_storage = source_aliases_current_storage(s, count, source_offset);
        if (count == 0) {
            return *this;
        }
        size_type old_size = m_size;
        size_type new_size = 0;
        if (!checked_growth(old_size, count, new_size,
                            "basic_string::append exceeds max_size")
            || !ensure_capacity(new_size)) {
            return *this;  /* grow failed, don't overflow */
        }
        if (source_aliases_storage)
            s = m_data + source_offset;
        for (size_type i = 0; i < count; i++) {
            m_data[old_size + i] = s[i];
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }

    basic_string& append(size_type count, CharT ch) {
        if (count == 0) {
            return *this;
        }
        size_type new_size = 0;
        if (!checked_growth(m_size, count, new_size,
                            "basic_string::append exceeds max_size")
            || !ensure_capacity(new_size)) {
            return *this;  /* grow failed */
        }
        for (size_type i = 0; i < count; i++) {
            m_data[m_size + i] = ch;
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }

    basic_string& append(initializer_list<CharT> values) {
        return append(values.begin(), values.end());
    }

    /* Iterator版append */
    template<typename InputIt>
    basic_string& append(InputIt first, InputIt last) {
        basic_string staged(m_alloc);
        for (; first != last; ++first) {
            size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*first));
            if (staged.size() == old_size) {
                return *this;
            }
        }
        return append(staged.data(), staged.size());
    }

#if __cplusplus > 202002L
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    basic_string& append_range(Range&& range) {
        return append(::std::begin(range), ::std::end(range));
    }
#endif

    void push_back(CharT ch) {
        size_type new_size = 0;
        if (!checked_growth(m_size, 1, new_size,
                            "basic_string::push_back exceeds max_size")
            || !ensure_capacity(new_size)) return;
        m_data[m_size] = ch;
        m_size = new_size;
        m_data[m_size] = CharT(0);
    }
    
    void pop_back() noexcept {
        if (m_size > 0) {
            m_size--;
            m_data[m_size] = CharT(0);
        }
    }
    
    /* 挿入 */
    basic_string& insert(size_type pos, const basic_string& str) {
        return insert(pos, str.m_data, str.m_size);
    }

    basic_string& insert(size_type pos, const basic_string& str,
                         size_type subpos, size_type subcount = npos) {
        if (subpos > str.m_size) {
            __detail::string_position_out_of_range(
                "basic_string::insert substring position is out of range");
        }
        const size_type available = str.m_size - subpos;
        const size_type count = (subcount == npos || subcount > available)
                              ? available : subcount;
        return insert(pos, str.m_data + subpos, count);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& insert(size_type pos, const StringViewLike& sv);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& insert(size_type pos, const StringViewLike& sv,
                         size_type subpos, size_type count = npos);
#endif
    
    basic_string& insert(size_type pos, const CharT* s) {
        return insert(pos, s, strlen(s));
    }
    
    basic_string& insert(size_type pos, const CharT* s, size_type count) {
        validate_pointer_count(s, count);
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::insert position is out of range");
        }
        if (count == 0) return *this;

        basic_string source_snapshot(m_alloc);
        size_type source_offset = 0;
        if (source_aliases_current_storage(s, count, source_offset)) {
            source_snapshot.assign(s, count);
            if (source_snapshot.size() != count) return *this;
            s = source_snapshot.data();
        }
        size_type new_size = 0;
        if (!checked_growth(m_size, count, new_size,
                            "basic_string::insert exceeds max_size")
            || !ensure_capacity(new_size)) return *this;
        
        /* 後ろにずらす */
        for (size_type i = m_size; i > pos; i--) {
            m_data[i + count - 1] = m_data[i - 1];
        }
        /* 挿入 */
        for (size_type i = 0; i < count; i++) {
            m_data[pos + i] = s[i];
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }
    
    basic_string& insert(size_type pos, size_type count, CharT ch) {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::insert position is out of range");
        }
        if (count == 0) return *this;
        size_type new_size = 0;
        if (!checked_growth(m_size, count, new_size,
                            "basic_string::insert exceeds max_size")
            || !ensure_capacity(new_size)) return *this;

        for (size_type i = m_size; i > pos; i--) {
            m_data[i + count - 1] = m_data[i - 1];
        }
        for (size_type i = 0; i < count; i++) {
            m_data[pos + i] = ch;
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }

    /* Iterator版insert */
    iterator insert(const_iterator pos, CharT ch) {
        size_type idx = iterator_index(pos);
        insert(idx, 1, ch);
        return m_data + idx;
    }

    iterator insert(const_iterator pos, size_type count, CharT ch) {
        size_type idx = iterator_index(pos);
        insert(idx, count, ch);
        return m_data + idx;
    }

    iterator insert(const_iterator pos, initializer_list<CharT> values) {
        size_type idx = iterator_index(pos);
        insert(m_data + idx, values.begin(), values.end());
        return m_data + idx;
    }

    template<typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        size_type idx = iterator_index(pos);
        basic_string staged(m_alloc);
        for (; first != last; ++first) {
            size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*first));
            if (staged.size() == old_size) {
                return m_data + idx;
            }
        }

        insert(idx, staged.data(), staged.size());
        return m_data + idx;
    }

#if __cplusplus > 202002L
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    iterator insert_range(const_iterator pos, Range&& range) {
        return insert(pos, ::std::begin(range), ::std::end(range));
    }
#endif

    /* 削除 */
    basic_string& erase(size_type pos = 0, size_type count = npos) {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::erase position is out of range");
        }
        if (pos == m_size) return *this;
        if (count == npos || count > m_size - pos) count = m_size - pos;

        for (size_type i = pos; i + count < m_size; i++) {
            m_data[i] = m_data[i + count];
        }
        m_size -= count;
        m_data[m_size] = CharT(0);
        return *this;
    }

    iterator erase(const_iterator position) {
        size_type pos = iterator_index(position);
        erase(pos, 1);
        return m_data + pos;
    }

    iterator erase(const_iterator first, const_iterator last) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        erase(pos, count);
        return m_data + pos;
    }

    /* 置換 */
    basic_string& replace(size_type pos, size_type count, const basic_string& str) {
        return replace(pos, count, str.m_data, str.m_size);
    }

    basic_string& replace(size_type pos, size_type count,
                          const basic_string& str, size_type subpos,
                          size_type subcount = npos) {
        if (subpos > str.m_size) {
            __detail::string_position_out_of_range(
                "basic_string::replace substring position is out of range");
        }
        const size_type available = str.m_size - subpos;
        const size_type replacement =
            (subcount == npos || subcount > available) ? available : subcount;
        return replace(pos, count, str.m_data + subpos, replacement);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& replace(size_type pos, size_type count,
                          const StringViewLike& sv);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& replace(size_type pos, size_type count,
                          const StringViewLike& sv, size_type subpos,
                          size_type count2 = npos);

    template<typename StringViewLike,
             typename enable_if<
                 __detail::is_compatible_string_view<
                     StringViewLike, CharT, Traits>::value,
                 int>::type = 0>
    basic_string& replace(const_iterator first, const_iterator last,
                          const StringViewLike& sv);
#endif
    
    basic_string& replace(size_type pos, size_type count, const CharT* s) {
        return replace(pos, count, s, strlen(s));
    }
    
    basic_string& replace(size_type pos, size_type count, const CharT* s, size_type count2) {
        validate_pointer_count(s, count2);
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::replace position is out of range");
        }
        if (count > m_size - pos) count = m_size - pos;

        basic_string source_snapshot(m_alloc);
        size_type source_offset = 0;
        if (source_aliases_current_storage(s, count2, source_offset)) {
            source_snapshot.assign(s, count2);
            if (source_snapshot.size() != count2) return *this;
            s = source_snapshot.data();
        }

        size_type new_size = 0;
        if (!checked_growth(m_size - count, count2, new_size,
                            "basic_string::replace exceeds max_size")
            || !ensure_capacity(new_size)) return *this;

        if (count2 > count) {
            /* 後ろに広げる */
            for (size_type i = m_size; i > pos + count; i--) {
                m_data[i + count2 - count - 1] = m_data[i - 1];
            }
        } else if (count2 < count) {
            /* 前に詰める */
            for (size_type i = pos + count; i < m_size; i++) {
                m_data[i - count + count2] = m_data[i];
            }
        }

        for (size_type i = 0; i < count2; i++) {
            m_data[pos + i] = s[i];
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }

    /* Replace with n copies of character ch */
    basic_string& replace(size_type pos, size_type count, size_type n, CharT ch) {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::replace position is out of range");
        }
        if (count > m_size - pos) count = m_size - pos;

        size_type new_size = 0;
        if (!checked_growth(m_size - count, n, new_size,
                            "basic_string::replace exceeds max_size")
            || !ensure_capacity(new_size)) return *this;

        if (n > count) {
            /* 後ろに広げる */
            for (size_type i = m_size; i > pos + count; i--) {
                m_data[i + n - count - 1] = m_data[i - 1];
            }
        } else if (n < count) {
            /* 前に詰める */
            for (size_type i = pos + count; i < m_size; i++) {
                m_data[i - count + n] = m_data[i];
            }
        }

        for (size_type i = 0; i < n; i++) {
            m_data[pos + i] = ch;
        }
        m_size = new_size;
        m_data[m_size] = CharT(0);
        return *this;
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          const basic_string& str) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, str);
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          const basic_string& str, size_type subpos,
                          size_type subcount = npos) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, str, subpos, subcount);
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          const CharT* s) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, s);
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          const CharT* s, size_type count2) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, s, count2);
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          size_type count2, CharT ch) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, count2, ch);
    }

    basic_string& replace(const_iterator first, const_iterator last,
                          initializer_list<CharT> values) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);
        return replace(pos, count, values.begin(), values.size());
    }

#if __cplusplus > 202002L
    /* C++23 range replacement must consume an input range exactly once and
     * publish the replacement only after the complete candidate exists.  In
     * particular, a range may alias this string; staging before replace()
     * keeps growth/shift from invalidating the source iterators. */
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    basic_string& replace_with_range(const_iterator first,
                                     const_iterator last,
                                     Range&& range) {
        size_type pos = 0;
        size_type count = 0;
        iterator_range_indices(first, last, pos, count);

        basic_string staged(m_alloc);
        auto range_first = ::std::begin(range);
        auto range_last = ::std::end(range);
        for (; range_first != range_last; ++range_first) {
            const size_type old_size = staged.size();
            staged.push_back(static_cast<CharT>(*range_first));
            if (staged.size() == old_size) return *this;
        }
        return replace(pos, count, staged.data(), staged.size());
    }

    /* The standard C++23 spelling is the ordinary replace overload with a
     * range as its third argument.  Keep the historical RinOS helper above as
     * a source-compatible alias, but expose the standard API as well.  The
     * defaulted begin/end expressions constrain this overload to range-like
     * objects and keep it out of the pointer/string overload set. */
    template<typename Range,
             typename = decltype(::std::begin(declval<Range&>())),
             typename = decltype(::std::end(declval<Range&>()))>
    basic_string& replace(const_iterator first,
                          const_iterator last,
                          Range&& range) {
        return replace_with_range(first, last, std::forward<Range>(range));
    }
#endif
    
    void resize(size_type count, CharT ch = CharT()) {
        if (!ensure_capacity(count)) return;
        if (count > m_size) {
            for (size_type i = m_size; i < count; i++) {
                m_data[i] = ch;
            }
        }
        m_size = count;
        m_data[m_size] = CharT(0);
    }

#if __cplusplus > 202002L
    /* C++23 resize_and_overwrite exposes the writable character buffer to a
     * caller-supplied operation after capacity has been established.  Keep
     * the operation bounded by the requested extent and publish the returned
     * size only after the operation has completed.  The standard precondition
     * requires the operation to return a value no greater than `count`; Rin
     * still terminates the candidate at `count` before reporting a contract
     * violation so a malformed callback cannot leave an unterminated string. */
    template<typename Operation>
    void resize_and_overwrite(size_type count, Operation operation) {
        /* Run the callback against a private, fully initialized candidate.
         * The C++23 operation is allowed to throw, and a callback can also
         * violate the returned-size contract.  Calling it on m_data directly
         * would let either failure mutate the old logical string even though
         * its size and terminator had not been published yet. */
        basic_string staged(m_alloc);
        if (!staged.ensure_capacity(count)) return;
        staged.m_size = count;
        const size_type preserved = m_size < count ? m_size : count;
        for (size_type index = 0; index < preserved; ++index)
            staged.m_data[index] = m_data[index];
        for (size_type index = preserved; index < count; ++index)
            staged.m_data[index] = CharT();
        staged.m_data[count] = CharT(0);

        const auto raw_result = operation(staged.m_data, count);
        if (is_signed<decltype(raw_result)>::value && raw_result < 0) {
            (void)__detail::string_length_failure(
                "basic_string::resize_and_overwrite result is negative");
            return;
        }
        const size_type result = static_cast<size_type>(raw_result);
        if (result > count) {
            (void)__detail::string_length_failure(
                "basic_string::resize_and_overwrite result exceeds count");
            return;
        }

        if (!ensure_capacity(count)) return;
        for (size_type index = 0; index < result; ++index)
            m_data[index] = staged.m_data[index];
        m_size = result;
        m_data[m_size] = CharT(0);
    }
#endif
    
    /* 検索 */
    size_type find(const basic_string& str, size_type pos = 0) const {
        return find(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    /* C++17 StringViewLike overloads.  A type that can also become a C
     * string deliberately keeps the long-standing pointer overload: its
     * terminator, rather than a possibly different view extent, defines that
     * operation. */
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type find(const StringViewLike& str, size_type pos = 0) const noexcept;
#endif
    
    size_type find(const CharT* s, size_type pos = 0) const {
        return find(s, pos, strlen(s));
    }
    
    size_type find(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        if (count == 0) return pos <= m_size ? pos : npos;
        if (pos > m_size || count > m_size - pos) return npos;
        
        for (size_type i = pos; i <= m_size - count; i++) {
            bool match = true;
            for (size_type j = 0; j < count; j++) {
                if (m_data[i + j] != s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i;
        }
        return npos;
    }
    
    size_type find(CharT ch, size_type pos = 0) const {
        for (size_type i = pos; i < m_size; i++) {
            if (m_data[i] == ch) return i;
        }
        return npos;
    }

#if __cplusplus >= 202002L
    bool starts_with(basic_string_view<CharT, Traits> sv) const noexcept;

    bool starts_with(CharT ch) const noexcept {
        return m_size != 0 && Traits::eq(m_data[0], ch);
    }

    bool starts_with(const CharT* s) const noexcept {
        if (!s) __builtin_trap();
        const size_type count = Traits::length(s);
        return count <= m_size && Traits::compare(m_data, s, count) == 0;
    }

    bool ends_with(basic_string_view<CharT, Traits> sv) const noexcept;

    bool ends_with(CharT ch) const noexcept {
        return m_size != 0 && Traits::eq(m_data[m_size - 1], ch);
    }

    bool ends_with(const CharT* s) const noexcept {
        if (!s) __builtin_trap();
        const size_type count = Traits::length(s);
        return count <= m_size &&
               Traits::compare(m_data + (m_size - count), s, count) == 0;
    }
#endif

#if __cplusplus > 202002L
    bool contains(basic_string_view<CharT, Traits> sv) const noexcept;

    bool contains(CharT ch) const noexcept {
        return find(ch) != npos;
    }

    bool contains(const CharT* s) const noexcept {
        if (!s) __builtin_trap();
        return find(s) != npos;
    }
#endif
    
    /* 逆方向検索 */
    size_type rfind(const basic_string& str, size_type pos = npos) const {
        return rfind(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type rfind(const StringViewLike& str, size_type pos = npos) const noexcept;
#endif
    
    size_type rfind(const CharT* s, size_type pos = npos) const {
        return rfind(s, pos, strlen(s));
    }
    
    size_type rfind(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        if (count == 0) return (pos < m_size) ? pos : m_size;
        if (count > m_size) return npos;
        
        size_type start = (pos == npos || pos > m_size - count) ? m_size - count : pos;
        for (size_type i = start + 1; i > 0; i--) {
            bool match = true;
            for (size_type j = 0; j < count; j++) {
                if (m_data[i - 1 + j] != s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i - 1;
        }
        return npos;
    }
    
    size_type rfind(CharT ch, size_type pos = npos) const {
        if (m_size == 0) return npos;
        size_type start = (pos >= m_size) ? m_size - 1 : pos;
        for (size_type i = start + 1; i > 0; i--) {
            if (m_data[i - 1] == ch) return i - 1;
        }
        return npos;
    }
    
    /* 最初の一致しない文字を探す */
    size_type find_first_not_of(const basic_string& str, size_type pos = 0) const {
        return find_first_not_of(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type find_first_not_of(const StringViewLike& str,
                                size_type pos = 0) const noexcept;
#endif
    
    size_type find_first_not_of(const CharT* s, size_type pos = 0) const {
        return find_first_not_of(s, pos, strlen(s));
    }
    
    size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        for (size_type i = pos; i < m_size; i++) {
            bool found = false;
            for (size_type j = 0; j < count; j++) {
                if (m_data[i] == s[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) return i;
        }
        return npos;
    }
    
    size_type find_first_not_of(CharT ch, size_type pos = 0) const {
        for (size_type i = pos; i < m_size; i++) {
            if (m_data[i] != ch) return i;
        }
        return npos;
    }
    
    size_type find_last_not_of(const basic_string& str, size_type pos = npos) const {
        return find_last_not_of(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type find_last_not_of(const StringViewLike& str,
                               size_type pos = npos) const noexcept;
#endif
    
    size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        if (m_size == 0) return npos;
        size_type start = (pos >= m_size) ? m_size - 1 : pos;
        for (size_type i = start + 1; i > 0; i--) {
            bool found = false;
            for (size_type j = 0; j < count; j++) {
                if (m_data[i - 1] == s[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) return i - 1;
        }
        return npos;
    }

    size_type find_last_not_of(const CharT* s, size_type pos = npos) const {
        return find_last_not_of(s, pos, strlen(s));
    }
    
    size_type find_first_of(const basic_string& str, size_type pos = 0) const {
        return find_first_of(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type find_first_of(const StringViewLike& str,
                            size_type pos = 0) const noexcept;
#endif
    
    size_type find_first_of(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        for (size_type i = pos; i < m_size; i++) {
            for (size_type j = 0; j < count; j++) {
                if (m_data[i] == s[j]) return i;
            }
        }
        return npos;
    }

    size_type find_first_of(const CharT* s, size_type pos = 0) const {
        return find_first_of(s, pos, strlen(s));
    }
    
    size_type find_first_of(CharT ch, size_type pos = 0) const {
        return find(ch, pos);
    }
    
    /* find_last_of: 指定文字のいずれかが最後に現れる位置 */
    size_type find_last_of(const basic_string& str, size_type pos = npos) const {
        return find_last_of(str.m_data, pos, str.m_size);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    size_type find_last_of(const StringViewLike& str,
                           size_type pos = npos) const noexcept;
#endif
    
    size_type find_last_of(const CharT* s, size_type pos, size_type count) const {
        validate_pointer_count(s, count);
        if (m_size == 0 || count == 0) return npos;
        size_type start = (pos >= m_size) ? m_size - 1 : pos;
        for (size_type i = start + 1; i > 0; ) {
            --i;
            for (size_type j = 0; j < count; j++) {
                if (m_data[i] == s[j]) return i;
            }
        }
        return npos;
    }
    
    size_type find_last_of(const CharT* s, size_type pos = npos) const {
        return find_last_of(s, pos, strlen(s));
    }
    
    size_type find_last_of(CharT ch, size_type pos = npos) const {
        return rfind(ch, pos);
    }
    
    /* 部分文字列 */
    size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::copy position is out of range");
        }
        size_type available = m_size - pos;
        size_type copied = count < available ? count : available;
        if (copied != 0) {
            /* A non-empty copy must have a destination.  The previous
             * implementation passed nullptr straight to Traits::copy,
             * turning a caller error into undefined memory access.  Keep
             * the zero-length contract (nullptr is never dereferenced) but
             * reject the invalid non-empty request before touching storage. */
            if (!dest) {
                __detail::string_position_out_of_range(
                    "basic_string::copy destination is null");
            }
            Traits::copy(dest, m_data + pos, copied);
        }
        return copied;
    }

    basic_string substr(size_type pos = 0, size_type count = npos) const & {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::substr position is out of range");
        }
        size_type available = m_size - pos;
        size_type len = (count == npos || count > available)
                      ? available : count;
        return basic_string(m_data + pos, len, m_alloc);
    }

#if __cplusplus > 202002L
    /* C++23 permits an rvalue substring to reuse the source allocation when
     * the requested view is the complete string.  Keep the allocator and
     * storage owner together by moving in that case; partial substrings still
     * use the checked copy constructor so the source remains valid until the
     * returned value is fully constructed. */
    basic_string substr(size_type pos = 0, size_type count = npos) && {
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::substr position is out of range");
        }
        size_type available = m_size - pos;
        size_type len = (count == npos || count > available)
                      ? available : count;
        if (pos == 0 && len == m_size) {
            return std::move(*this);
        }
        return basic_string(m_data + pos, len, m_alloc);
    }
#endif
    
    /* 比較 */
    int compare(const basic_string& str) const {
        return compare_data(m_data, m_size, str.m_data, str.m_size);
    }

    int compare(size_type pos, size_type count, const basic_string& str) const {
        return compare(pos, count, str.m_data, str.m_size);
    }

    int compare(size_type pos1, size_type count1, const basic_string& str,
                size_type pos2, size_type count2 = npos) const {
        if (pos2 > str.m_size) {
            __detail::string_position_out_of_range(
                "basic_string::compare position is out of range");
        }
        const size_type available = str.m_size - pos2;
        const size_type length = (count2 == npos || count2 > available)
                               ? available : count2;
        return compare(pos1, count1, str.m_data + pos2, length);
    }

#if __cplusplus >= 201703L
    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    int compare(const StringViewLike& str) const noexcept;

    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    int compare(size_type pos, size_type count, const StringViewLike& str) const;

    template<typename StringViewLike,
             typename enable_if<
                 is_convertible<const StringViewLike&,
                                basic_string_view<CharT, Traits>>::value &&
                 !is_convertible<const StringViewLike&, const CharT*>::value,
                 int>::type = 0>
    int compare(size_type pos1, size_type count1, const StringViewLike& str,
                size_type pos2, size_type count2 = npos) const;
#endif

    int compare(const CharT* s) const {
        return compare_data(m_data, m_size, s, strlen(s));
    }

    int compare(size_type pos, size_type count, const CharT* s) const {
        /* Compare the selected range directly.  Building a temporary
         * substring made this observer unexpectedly allocate and could turn
         * an allocation failure into a bogus comparison result. */
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::compare position is out of range");
        }
        return compare(pos, count, s, strlen(s));
    }

    int compare(size_type pos, size_type count, const CharT* s, size_type s_count) const {
        validate_pointer_count(s, s_count);
        if (pos > m_size) {
            __detail::string_position_out_of_range(
                "basic_string::compare position is out of range");
        }
        size_type available = m_size - pos;
        size_type my_len = (count == npos || count > available)
                         ? available : count;
        return compare_data(m_data + pos, my_len, s, s_count);
    }

    bool operator==(const basic_string& other) const { return compare(other) == 0; }
    bool operator!=(const basic_string& other) const { return compare(other) != 0; }
    bool operator<(const basic_string& other) const { return compare(other) < 0; }
    bool operator>(const basic_string& other) const { return compare(other) > 0; }
    bool operator<=(const basic_string& other) const { return compare(other) <= 0; }
    bool operator>=(const basic_string& other) const { return compare(other) >= 0; }

    /* The allocator is not part of a string's value.  Keep the non-template
     * same-specialization members above for source compatibility, and add
     * the standard cross-allocator forms without requiring a private access
     * friendship between specializations. */
    template<typename OtherAllocator>
    bool operator==(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return m_size == other.size()
            && compare_data(m_data, m_size, other.data(), other.size()) == 0;
    }
    template<typename OtherAllocator>
    bool operator!=(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return !(*this == other);
    }
    template<typename OtherAllocator>
    bool operator<(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return compare_data(m_data, m_size, other.data(), other.size()) < 0;
    }
    template<typename OtherAllocator>
    bool operator>(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return other < *this;
    }
    template<typename OtherAllocator>
    bool operator<=(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return !(*this > other);
    }
    template<typename OtherAllocator>
    bool operator>=(const basic_string<CharT, Traits, OtherAllocator>& other) const {
        return !(*this < other);
    }
#if __cplusplus >= 202002L
    template<typename OtherAllocator>
    strong_ordering operator<=>
        (const basic_string<CharT, Traits, OtherAllocator>& other) const noexcept {
        const int result = compare_data(m_data, m_size,
                                        other.data(), other.size());
        return result < 0 ? strong_ordering::less
             : result > 0 ? strong_ordering::greater
                          : strong_ordering::equal;
    }
#endif
};

#if __cplusplus < 201703L
template<typename CharT, typename Traits, typename Allocator>
CharT basic_string<CharT, Traits, Allocator>::s_empty_storage[1] = {};
#endif

/* 文字列結合 */
template<typename CharT>
basic_string<CharT> operator+(const basic_string<CharT>& lhs, const basic_string<CharT>& rhs) {
    basic_string<CharT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT>
basic_string<CharT> operator+(const basic_string<CharT>& lhs, const CharT* rhs) {
    basic_string<CharT> result = lhs;
    result += rhs;
    return result;
}

template<typename CharT>
basic_string<CharT> operator+(const CharT* lhs, const basic_string<CharT>& rhs) {
    basic_string<CharT> result(lhs);
    result += rhs;
    return result;
}

/* Allocator-aware lvalue concatenation.  The legacy overloads above retain
 * the default-allocator ABI; these templates cover custom allocator strings
 * and keep the left string's allocator as the result owner. */
template<typename CharT, typename Traits, typename LeftAllocator,
         typename RightAllocator,
         typename enable_if<
             !is_same<LeftAllocator, allocator<CharT>>::value ||
             !is_same<RightAllocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, LeftAllocator> operator+
    (const basic_string<CharT, Traits, LeftAllocator>& lhs,
     const basic_string<CharT, Traits, RightAllocator>& rhs) {
    basic_string<CharT, Traits, LeftAllocator> result(lhs);
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (const basic_string<CharT, Traits, Allocator>& lhs,
     const CharT* rhs) {
    basic_string<CharT, Traits, Allocator> result(lhs);
    result.append(rhs);
    return result;
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (const CharT* lhs,
     const basic_string<CharT, Traits, Allocator>& rhs) {
    basic_string<CharT, Traits, Allocator> result(lhs, rhs.get_allocator());
    result.append(rhs.data(), rhs.size());
    return result;
}

/* string + char */
template<typename CharT>
basic_string<CharT> operator+(const basic_string<CharT>& lhs, CharT ch) {
    basic_string<CharT> result = lhs;
    result += ch;
    return result;
}

template<typename CharT>
basic_string<CharT> operator+(CharT ch, const basic_string<CharT>& rhs) {
    basic_string<CharT> result(1, ch);
    result += rhs;
    return result;
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (const basic_string<CharT, Traits, Allocator>& lhs, CharT rhs) {
    basic_string<CharT, Traits, Allocator> result(lhs);
    result += rhs;
    return result;
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (CharT lhs, const basic_string<CharT, Traits, Allocator>& rhs) {
    basic_string<CharT, Traits, Allocator> result(1, lhs,
                                                   rhs.get_allocator());
    result.append(rhs.data(), rhs.size());
    return result;
}

/* rvalue overloads for efficiency */
template<typename CharT>
basic_string<CharT> operator+(basic_string<CharT>&& lhs, const basic_string<CharT>& rhs) {
    lhs += rhs;
    return std::move(lhs);
}

template<typename CharT>
basic_string<CharT> operator+(basic_string<CharT>&& lhs, const CharT* rhs) {
    lhs += rhs;
    return std::move(lhs);
}

template<typename CharT>
basic_string<CharT> operator+(basic_string<CharT>&& lhs, CharT ch) {
    lhs += ch;
    return std::move(lhs);
}

/* The standard also permits the right operand to be the movable string.  A
 * missing overload here silently selected the const-reference path, which
 * needlessly copied and made the result incorrect for move-sensitive custom
 * allocators.  Mutate the movable operand when the left side is const, and
 * otherwise append into the left operand just like the existing rvalue
 * overloads. */
template<typename CharT>
basic_string<CharT> operator+(const basic_string<CharT>& lhs,
                              basic_string<CharT>&& rhs) {
    rhs.insert(0, lhs);
    return std::move(rhs);
}

template<typename CharT>
basic_string<CharT> operator+(basic_string<CharT>&& lhs,
                              basic_string<CharT>&& rhs) {
    lhs += rhs;
    return std::move(lhs);
}

template<typename CharT>
basic_string<CharT> operator+(const CharT* lhs, basic_string<CharT>&& rhs) {
    basic_string<CharT> result(lhs);
    result += rhs;
    return result;
}

template<typename CharT>
basic_string<CharT> operator+(CharT ch, basic_string<CharT>&& rhs) {
    typedef typename basic_string<CharT>::size_type size_type;
    rhs.insert(static_cast<size_type>(0), static_cast<size_type>(1), ch);
    return std::move(rhs);
}

/* Reuse custom-allocator rvalues in the same way as the default-allocator
 * overloads. Append by public view when allocator types differ, so no
 * cross-specialization private access or allocator ownership transfer is
 * required. */
template<typename CharT, typename Traits, typename LeftAllocator,
         typename RightAllocator,
         typename enable_if<
             !is_same<LeftAllocator, allocator<CharT>>::value ||
             !is_same<RightAllocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, LeftAllocator> operator+
    (basic_string<CharT, Traits, LeftAllocator>&& lhs,
     const basic_string<CharT, Traits, RightAllocator>& rhs) {
    lhs.append(rhs.data(), rhs.size());
    return std::move(lhs);
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (basic_string<CharT, Traits, Allocator>&& lhs, const CharT* rhs) {
    lhs.append(rhs);
    return std::move(lhs);
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (basic_string<CharT, Traits, Allocator>&& lhs, CharT rhs) {
    lhs += rhs;
    return std::move(lhs);
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (const CharT* lhs,
     basic_string<CharT, Traits, Allocator>&& rhs) {
    rhs.insert(static_cast<typename basic_string<CharT, Traits, Allocator>::size_type>(0),
               lhs);
    return std::move(rhs);
}

template<typename CharT, typename Traits, typename Allocator,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, Traits, Allocator> operator+
    (CharT lhs, basic_string<CharT, Traits, Allocator>&& rhs) {
    rhs.insert(static_cast<typename basic_string<CharT, Traits, Allocator>::size_type>(0),
               static_cast<typename basic_string<CharT, Traits, Allocator>::size_type>(1),
               lhs);
    return std::move(rhs);
}

#if __cplusplus >= 201703L
/* C++17 string/string_view concatenation.  The earlier string overload set
 * had no direct view operands, so overload resolution either failed or took
 * an unintended temporary conversion.  Keep the result in the string's
 * existing checked append/insert paths and preserve rvalue reuse where the
 * standard permits it. */
template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    const basic_string<CharT>& lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    basic_string<CharT> result(lhs);
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    const basic_string<CharT>& rhs) {
    basic_string<CharT> result(lhs.data(), lhs.size());
    result += rhs;
    return result;
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    basic_string<CharT>&& lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    lhs.append(rhs.data(), rhs.size());
    return std::move(lhs);
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    basic_string<CharT>&& rhs) {
    rhs.insert(static_cast<typename basic_string<CharT>::size_type>(0),
               lhs.data(), lhs.size());
    return std::move(rhs);
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    const CharT* lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    basic_string<CharT> result(lhs);
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    const CharT* rhs) {
    basic_string<CharT> result(lhs.data(), lhs.size());
    result += rhs;
    return result;
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    CharT lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    basic_string<CharT> result(static_cast<typename basic_string<CharT>::size_type>(1), lhs);
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename ViewTraits>
basic_string<CharT> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    CharT rhs) {
    basic_string<CharT> result(lhs.data(), lhs.size());
    result += rhs;
    return result;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, StringTraits, Allocator> operator+(
    const basic_string<CharT, StringTraits, Allocator>& lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    basic_string<CharT, StringTraits, Allocator> result(lhs);
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, StringTraits, Allocator> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    const basic_string<CharT, StringTraits, Allocator>& rhs) {
    basic_string<CharT, StringTraits, Allocator> result(lhs.data(), lhs.size(),
                                                         rhs.get_allocator());
    result.append(rhs.data(), rhs.size());
    return result;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, StringTraits, Allocator> operator+(
    basic_string<CharT, StringTraits, Allocator>&& lhs,
    basic_string_view<CharT, ViewTraits> rhs) {
    lhs.append(rhs.data(), rhs.size());
    return std::move(lhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits,
         typename enable_if<
             !is_same<Allocator, allocator<CharT>>::value,
             int>::type = 0>
basic_string<CharT, StringTraits, Allocator> operator+(
    basic_string_view<CharT, ViewTraits> lhs,
    basic_string<CharT, StringTraits, Allocator>&& rhs) {
    rhs.insert(static_cast<typename basic_string<CharT, StringTraits, Allocator>::size_type>(0),
               lhs.data(), lhs.size());
    return std::move(rhs);
}
#endif

/* 非メンバー比較演算子 */
template<typename CharT, typename StringTraits, typename Allocator>
bool operator==(const basic_string<CharT, StringTraits, Allocator>& lhs,
                const CharT* rhs) {
    return lhs.compare(rhs) == 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator==(const CharT* lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) == 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator!=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                const CharT* rhs) {
    return lhs.compare(rhs) != 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator!=(const CharT* lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) != 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator<(const basic_string<CharT, StringTraits, Allocator>& lhs,
               const CharT* rhs) {
    return lhs.compare(rhs) < 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator<(const CharT* lhs,
               const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) > 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator>(const basic_string<CharT, StringTraits, Allocator>& lhs,
               const CharT* rhs) {
    return lhs.compare(rhs) > 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator>(const CharT* lhs,
               const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) < 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator<=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                const CharT* rhs) {
    return lhs.compare(rhs) <= 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator<=(const CharT* lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) >= 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator>=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                const CharT* rhs) {
    return lhs.compare(rhs) >= 0;
}

template<typename CharT, typename StringTraits, typename Allocator>
bool operator>=(const CharT* lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(lhs) <= 0;
}

/* string vs string_view comparison operators */
template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator==(const basic_string<CharT, StringTraits, Allocator>& lhs,
                basic_string_view<CharT, ViewTraits> rhs) {
    return lhs.size() == rhs.size() && lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator==(basic_string_view<CharT, ViewTraits> lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.size() == lhs.size() && rhs.compare(0, rhs.size(), lhs.data(), lhs.size()) == 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator!=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                basic_string_view<CharT, ViewTraits> rhs) {
    return !(lhs == rhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator!=(basic_string_view<CharT, ViewTraits> lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator<(const basic_string<CharT, StringTraits, Allocator>& lhs,
               basic_string_view<CharT, ViewTraits> rhs) {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) < 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator<(basic_string_view<CharT, ViewTraits> lhs,
               const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(0, rhs.size(), lhs.data(), lhs.size()) > 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator>(const basic_string<CharT, StringTraits, Allocator>& lhs,
               basic_string_view<CharT, ViewTraits> rhs) {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) > 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator>(basic_string_view<CharT, ViewTraits> lhs,
               const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return rhs.compare(0, rhs.size(), lhs.data(), lhs.size()) < 0;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator<=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                basic_string_view<CharT, ViewTraits> rhs) {
    return !(lhs > rhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator<=(basic_string_view<CharT, ViewTraits> lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return !(lhs > rhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator>=(const basic_string<CharT, StringTraits, Allocator>& lhs,
                basic_string_view<CharT, ViewTraits> rhs) {
    return !(lhs < rhs);
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
bool operator>=(basic_string_view<CharT, ViewTraits> lhs,
                const basic_string<CharT, StringTraits, Allocator>& rhs) {
    return !(lhs < rhs);
}

#if __cplusplus >= 202002L
/* C++20 non-member string erasure helpers.  Keep the mutation in the
 * string's checked iterator/erase path so foreign iterators and the empty
 * string boundary retain the same diagnostics as member erasure. */
template<typename CharT, typename Traits, typename Allocator>
typename basic_string<CharT, Traits, Allocator>::size_type
erase(basic_string<CharT, Traits, Allocator>& value, const CharT& character) {
    typedef basic_string<CharT, Traits, Allocator> string_type;
    typedef typename string_type::size_type size_type;
    const size_type old_size = value.size();
    typename string_type::iterator current = value.begin();
    while (current != value.end()) {
        if (*current == character) {
            current = value.erase(current);
        } else {
            ++current;
        }
    }
    return old_size - value.size();
}

template<typename CharT, typename Traits, typename Allocator, typename Pred>
typename basic_string<CharT, Traits, Allocator>::size_type
erase_if(basic_string<CharT, Traits, Allocator>& value, Pred predicate) {
    typedef basic_string<CharT, Traits, Allocator> string_type;
    typedef typename string_type::size_type size_type;
    const size_type old_size = value.size();
    typename string_type::iterator current = value.begin();
    while (current != value.end()) {
        if (predicate(*current)) {
            current = value.erase(current);
        } else {
            ++current;
        }
    }
    return old_size - value.size();
}
#endif

#if __cplusplus >= 202002L
/* C++20's three-way string comparisons use the same code-unit ordering as
 * the legacy relational family.  Keep the result strongly ordered: a
 * basic_string comparison is lexicographical and cannot be unordered. */
template<typename CharT, typename Traits, typename Allocator>
strong_ordering operator<=>(
    const basic_string<CharT, Traits, Allocator>& lhs,
    const basic_string<CharT, Traits, Allocator>& rhs) noexcept {
    const int result = lhs.compare(rhs);
    return result < 0 ? strong_ordering::less
         : result > 0 ? strong_ordering::greater
                      : strong_ordering::equal;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
strong_ordering operator<=>
    (const basic_string<CharT, StringTraits, Allocator>& lhs,
     basic_string_view<CharT, ViewTraits> rhs) noexcept {
    const int result = lhs.compare(0, lhs.size(), rhs.data(), rhs.size());
    return result < 0 ? strong_ordering::less
         : result > 0 ? strong_ordering::greater
                      : strong_ordering::equal;
}

template<typename CharT, typename StringTraits, typename Allocator,
         typename ViewTraits>
strong_ordering operator<=>
    (basic_string_view<CharT, ViewTraits> lhs,
     const basic_string<CharT, StringTraits, Allocator>& rhs) noexcept {
    const int result = rhs.compare(0, rhs.size(), lhs.data(), lhs.size());
    return result < 0 ? strong_ordering::greater
         : result > 0 ? strong_ordering::less
                      : strong_ordering::equal;
}

/* A string literal cannot participate in the string/string_view spaceship
 * overloads above: template argument deduction does not consider the
 * user-defined conversion from an array to basic_string_view.  Provide the
 * standard C-string forms explicitly and keep comparison in the existing
 * non-allocating compare() owner. */
template<typename CharT, typename StringTraits, typename Allocator>
strong_ordering operator<=>
    (const basic_string<CharT, StringTraits, Allocator>& lhs,
     const CharT* rhs) noexcept {
    const int result = lhs.compare(rhs);
    return result < 0 ? strong_ordering::less
         : result > 0 ? strong_ordering::greater
                      : strong_ordering::equal;
}

template<typename CharT, typename StringTraits, typename Allocator>
strong_ordering operator<=>
    (const CharT* lhs,
     const basic_string<CharT, StringTraits, Allocator>& rhs) noexcept {
    const int result = rhs.compare(lhs);
    return result < 0 ? strong_ordering::greater
         : result > 0 ? strong_ordering::less
                      : strong_ordering::equal;
}
#endif

/* エイリアス */
using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
using u16string = basic_string<char16_t>;
using u32string = basic_string<char32_t>;
#if defined(__cpp_char8_t)
using u8string = basic_string<char8_t>;
#endif

/* 数値変換 */
namespace __detail {

template<typename Integer>
inline string integer_to_string(Integer value) {
    char buffer[64];
    unsigned long long magnitude;
    bool negative = false;
    if (is_signed<Integer>::value && value < 0) {
        negative = true;
        /* Convert to unsigned before negating so the minimum signed value
         * has a defined two's-complement magnitude. */
        magnitude = 0ull - static_cast<unsigned long long>(value);
    } else {
        magnitude = static_cast<unsigned long long>(value);
    }

    size_t length = 0;
    do {
        buffer[length++] = static_cast<char>('0' + (magnitude % 10ull));
        magnitude /= 10ull;
    } while (magnitude != 0ull);
    if (negative) buffer[length++] = '-';

    string result;
    while (length != 0) result += buffer[--length];
    return result;
}

} /* namespace __detail */

inline string to_string(int value) {
    return __detail::integer_to_string(value);
}

inline string to_string(unsigned int value) {
    return __detail::integer_to_string(value);
}

inline string to_string(long value) { return __detail::integer_to_string(value); }
inline string to_string(unsigned long value) {
    return __detail::integer_to_string(value);
}
inline string to_string(long long value) {
    return __detail::integer_to_string(value);
}
inline string to_string(unsigned long long value) {
    return __detail::integer_to_string(value);
}

namespace __detail {

/* std::to_string uses printf-like fixed formatting with six fractional
 * digits.  Convert the IEEE-754 binary value to a decimal fixed-point
 * integer first, so large magnitudes never pass through an overflowing
 * signed cast and fractional rounding does not accumulate floating error. */
static const size_t fixed_decimal_limb_capacity = 40u;
static const uint32_t fixed_decimal_base = 1000000000u;

struct fixed_decimal_integer {
    uint32_t limbs[fixed_decimal_limb_capacity];
    size_t count;
};

inline void fixed_decimal_zero(fixed_decimal_integer& value) {
    for (size_t index = 0; index < fixed_decimal_limb_capacity; ++index)
        value.limbs[index] = 0u;
    value.count = 0u;
}

inline void fixed_decimal_normalize(fixed_decimal_integer& value) {
    while (value.count != 0u && value.limbs[value.count - 1u] == 0u)
        --value.count;
}

inline void fixed_decimal_from_mantissa(fixed_decimal_integer& value,
                                        uint64_t mantissa) {
    fixed_decimal_zero(value);
    while (mantissa != 0u) {
        value.limbs[value.count++] =
            static_cast<uint32_t>(mantissa % fixed_decimal_base);
        mantissa /= fixed_decimal_base;
    }
}

inline bool fixed_decimal_mul_small(fixed_decimal_integer& value,
                                    uint32_t multiplier) {
    uint64_t carry = 0u;
    for (size_t index = 0; index < value.count; ++index) {
        const uint64_t product = static_cast<uint64_t>(value.limbs[index]) *
                                 multiplier + carry;
        value.limbs[index] = static_cast<uint32_t>(product % fixed_decimal_base);
        carry = product / fixed_decimal_base;
    }
    while (carry != 0u) {
        if (value.count == fixed_decimal_limb_capacity) return false;
        value.limbs[value.count++] =
            static_cast<uint32_t>(carry % fixed_decimal_base);
        carry /= fixed_decimal_base;
    }
    return true;
}

inline bool fixed_decimal_add_one(fixed_decimal_integer& value) {
    size_t index = 0u;
    uint64_t carry = 1u;
    while (carry != 0u) {
        if (index == value.count) {
            if (value.count == fixed_decimal_limb_capacity) return false;
            value.limbs[value.count++] = 0u;
        }
        const uint64_t sum = static_cast<uint64_t>(value.limbs[index]) + carry;
        value.limbs[index] = static_cast<uint32_t>(sum % fixed_decimal_base);
        carry = sum / fixed_decimal_base;
        ++index;
    }
    return true;
}

inline bool fixed_decimal_shift_left(fixed_decimal_integer& value,
                                     unsigned count) {
    while (count-- != 0u) {
        uint64_t carry = 0u;
        for (size_t index = 0; index < value.count; ++index) {
            const uint64_t doubled =
                static_cast<uint64_t>(value.limbs[index]) * 2u + carry;
            value.limbs[index] = static_cast<uint32_t>(doubled % fixed_decimal_base);
            carry = doubled / fixed_decimal_base;
        }
        if (carry != 0u) {
            if (value.count == fixed_decimal_limb_capacity) return false;
            value.limbs[value.count++] = static_cast<uint32_t>(carry);
        }
    }
    return true;
}

inline bool fixed_decimal_low_bit(const fixed_decimal_integer& value) {
    return value.count != 0u && (value.limbs[0] & 1u) != 0u;
}

inline void fixed_decimal_shift_right_one(fixed_decimal_integer& value) {
    uint32_t carry = 0u;
    for (size_t index = value.count; index != 0u; --index) {
        const uint32_t current = value.limbs[index - 1u];
        value.limbs[index - 1u] = (current / 2u) +
            (carry == 0u ? 0u : fixed_decimal_base / 2u);
        carry = current % 2u;
    }
    fixed_decimal_normalize(value);
}

inline uint32_t fixed_decimal_div_small(fixed_decimal_integer& value,
                                        uint32_t divisor) {
    uint64_t remainder = 0u;
    for (size_t index = value.count; index != 0u; --index) {
        const uint64_t current = remainder * fixed_decimal_base +
                                 value.limbs[index - 1u];
        value.limbs[index - 1u] = static_cast<uint32_t>(current / divisor);
        remainder = current % divisor;
    }
    fixed_decimal_normalize(value);
    return static_cast<uint32_t>(remainder);
}

inline string fixed_float_to_string(double input) {
    union {
        double value;
        uint64_t bits;
    } raw = {input};
    const bool negative = (raw.bits >> 63u) != 0u;
    const uint64_t exponent_bits = (raw.bits >> 52u) & 0x7ffu;
    const uint64_t fraction_bits = raw.bits & UINT64_C(0x000fffffffffffff);
    if (exponent_bits == 0x7ffu) {
        if (fraction_bits != 0u) return string(negative ? "-nan" : "nan");
        return string(negative ? "-inf" : "inf");
    }

    const uint64_t mantissa = exponent_bits == 0u
        ? fraction_bits : (UINT64_C(0x0010000000000000) | fraction_bits);
    fixed_decimal_integer scaled;
    fixed_decimal_from_mantissa(scaled, mantissa);
    if (!fixed_decimal_mul_small(scaled, 1000000u)) return string();
    const int binary_exponent = exponent_bits == 0u
        ? -1074 : static_cast<int>(exponent_bits) - 1023 - 52;
    if (binary_exponent >= 0) {
        if (!fixed_decimal_shift_left(scaled,
                                      static_cast<unsigned>(binary_exponent)))
            return string();
    } else {
        bool discarded = false;
        bool halfway = false;
        const unsigned shift = static_cast<unsigned>(-binary_exponent);
        for (unsigned index = 0u; index < shift; ++index) {
            const bool bit = fixed_decimal_low_bit(scaled);
            if (index + 1u == shift) halfway = bit;
            else if (bit) discarded = true;
            fixed_decimal_shift_right_one(scaled);
        }
        if (halfway && (discarded || fixed_decimal_low_bit(scaled)))
            if (!fixed_decimal_add_one(scaled)) return string();
    }

    fixed_decimal_integer integer_part = scaled;
    const uint32_t fractional = fixed_decimal_div_small(integer_part, 1000000u);
    char reversed[384];
    size_t integer_length = 0u;
    while (integer_part.count != 0u) {
        if (integer_length == sizeof(reversed)) return string();
        reversed[integer_length++] = static_cast<char>(
            '0' + fixed_decimal_div_small(integer_part, 10u));
    }
    if (integer_length == 0u) reversed[integer_length++] = '0';

    char fractional_digits[6];
    uint32_t divisor = 100000u;
    uint32_t remainder = fractional;
    for (size_t index = 0u; index < 6u; ++index) {
        fractional_digits[index] = static_cast<char>('0' + remainder / divisor);
        remainder %= divisor;
        divisor /= 10u;
    }

    string result;
    if (negative) result += '-';
    while (integer_length != 0u) result += reversed[--integer_length];
    result += '.';
    for (size_t index = 0u; index < 6u; ++index)
        result += fractional_digits[index];
    return result;
}

} /* namespace __detail */

inline string to_string(double value) {
    return __detail::fixed_float_to_string(value);
}
inline string to_string(float value) { return to_string(static_cast<double>(value)); }

namespace __detail {

template<typename Integer>
inline wstring integer_to_wstring(Integer value) {
    const string narrow = integer_to_string(value);
    wstring result;
    for (size_t i = 0; i < narrow.size(); ++i) {
        result += static_cast<wchar_t>(static_cast<unsigned char>(narrow[i]));
    }
    return result;
}

inline wstring narrow_to_wstring(const string& narrow) {
    wstring result;
    for (size_t i = 0; i < narrow.size(); ++i) {
        result += static_cast<wchar_t>(static_cast<unsigned char>(narrow[i]));
    }
    return result;
}

} /* namespace __detail */

inline wstring to_wstring(int value) {
    return __detail::integer_to_wstring(value);
}

inline wstring to_wstring(unsigned int value) {
    return __detail::integer_to_wstring(value);
}

inline wstring to_wstring(long value) {
    return __detail::integer_to_wstring(value);
}

inline wstring to_wstring(unsigned long value) {
    return __detail::integer_to_wstring(value);
}

inline wstring to_wstring(long long value) {
    return __detail::integer_to_wstring(value);
}

inline wstring to_wstring(unsigned long long value) {
    return __detail::integer_to_wstring(value);
}

namespace __detail {

struct string_integer_parse {
    unsigned long long magnitude;
    size_t end;
    bool negative;
    bool any;
    bool overflow;
};

inline bool string_integer_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\f' || c == '\v';
}

inline int string_integer_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

inline string_integer_parse parse_string_integer(
    const string& str, int base, unsigned long long positive_limit,
    unsigned long long negative_limit) {
    string_integer_parse result = {0ull, 0, false, false, false};
    const char* s = str.c_str();
    const size_t length = str.size();
    size_t i = 0;
    while (i < length && string_integer_space(s[i])) ++i;

    if (i < length && s[i] == '-') {
        result.negative = true;
        ++i;
    } else if (i < length && s[i] == '+') {
        ++i;
    }

    if (base != 0 && (base < 2 || base > 36)) {
        string_numeric_invalid("invalid integer base");
    }

    int digit_base = base;
    if (digit_base == 0) {
        digit_base = 10;
        if (i < length && s[i] == '0') {
            digit_base = 8;
            if (i + 2 < length && (s[i + 1] == 'x' || s[i + 1] == 'X')
                && string_integer_digit(s[i + 2]) >= 0
                && string_integer_digit(s[i + 2]) < 16) {
                digit_base = 16;
                i += 2;
            }
        }
    } else if (digit_base == 16 && i + 2 < length && s[i] == '0'
               && (s[i + 1] == 'x' || s[i + 1] == 'X')
               && string_integer_digit(s[i + 2]) >= 0
               && string_integer_digit(s[i + 2]) < 16) {
        i += 2;
    }

    const unsigned long long limit = result.negative
        ? negative_limit : positive_limit;
    const unsigned long long base_value =
        static_cast<unsigned long long>(digit_base);
    const unsigned long long cutoff = limit / base_value;
    const unsigned long long cutlim = limit % base_value;
    while (i < length) {
        const int digit = string_integer_digit(s[i]);
        if (digit < 0 || digit >= digit_base) break;
        result.any = true;
        if (result.magnitude > cutoff
            || (result.magnitude == cutoff
                && static_cast<unsigned long long>(digit) > cutlim)) {
            result.overflow = true;
        } else if (!result.overflow) {
            result.magnitude = result.magnitude * base_value
                + static_cast<unsigned long long>(digit);
        }
        ++i;
    }
    result.end = i;
    if (!result.any) {
        string_numeric_invalid("invalid integer");
    }
    if (result.overflow) {
        string_numeric_range("integer conversion out of range");
    }
    return result;
}

inline unsigned long long string_ulong_max() {
    return sizeof(unsigned long) == 4 ? 0xffffffffull : ~0ull;
}

inline unsigned long long string_long_max() {
    return sizeof(long) == 4 ? 0x7fffffffull : 0x7fffffffffffffffull;
}

inline unsigned long long string_long_min_magnitude() {
    return string_long_max() + 1ull;
}

template<typename Signed>
inline Signed string_signed_result(const string_integer_parse& parsed) {
    typedef typename make_unsigned<Signed>::type unsigned_type;
    const unsigned_type magnitude = static_cast<unsigned_type>(parsed.magnitude);
    if (parsed.negative) {
        return static_cast<Signed>(static_cast<unsigned_type>(0) - magnitude);
    }
    return static_cast<Signed>(magnitude);
}

} /* namespace __detail */

inline int stoi(const string& str, size_t* pos = nullptr, int base = 10) {
    const __detail::string_integer_parse parsed = __detail::parse_string_integer(
        str, base, 0x7fffffffull, 0x80000000ull);
    if (pos) *pos = parsed.end;
    return __detail::string_signed_result<int>(parsed);
}

inline long stol(const string& str, size_t* pos = nullptr, int base = 10) {
    const __detail::string_integer_parse parsed = __detail::parse_string_integer(
        str, base, __detail::string_long_max(),
        __detail::string_long_min_magnitude());
    if (pos) *pos = parsed.end;
    return __detail::string_signed_result<long>(parsed);
}

inline unsigned long stoul(const string& str, size_t* pos = nullptr, int base = 10) {
    const __detail::string_integer_parse parsed = __detail::parse_string_integer(
        str, base, __detail::string_ulong_max(), __detail::string_ulong_max());
    if (pos) *pos = parsed.end;
    typedef unsigned long unsigned_type;
    const unsigned_type magnitude = static_cast<unsigned_type>(parsed.magnitude);
    return parsed.negative
        ? static_cast<unsigned_type>(static_cast<unsigned_type>(0) - magnitude)
        : magnitude;
}

inline long long stoll(const string& str, size_t* pos = nullptr, int base = 10) {
    const __detail::string_integer_parse parsed = __detail::parse_string_integer(
        str, base, 0x7fffffffffffffffull, 0x8000000000000000ull);
    if (pos) *pos = parsed.end;
    return __detail::string_signed_result<long long>(parsed);
}

inline unsigned long long stoull(const string& str, size_t* pos = nullptr,
                                 int base = 10) {
    const __detail::string_integer_parse parsed = __detail::parse_string_integer(
        str, base, ~0ull, ~0ull);
    if (pos) *pos = parsed.end;
    return parsed.negative ? 0ull - parsed.magnitude : parsed.magnitude;
}

inline double stod(const string& str, size_t* pos = nullptr) {
    const char* begin = str.c_str();
    const rin_float_parse_result parsed = rin_float_parse_binary64(begin);
    if (!parsed.converted) {
        __detail::string_numeric_invalid("invalid floating-point value");
    }
    if (parsed.range_direction != 0 || parsed.subnormal) {
        __detail::string_numeric_range("floating-point conversion out of range");
    }
    if (pos) *pos = static_cast<size_t>(parsed.end - begin);
    return rin_float_parse_binary64_value(parsed.bits);
}

inline float stof(const string& str, size_t* pos = nullptr) {
    const char* begin = str.c_str();
    const rin_float_parse_result parsed = rin_float_parse_binary32(begin);
    if (!parsed.converted) {
        __detail::string_numeric_invalid("invalid floating-point value");
    }
    if (parsed.range_direction != 0 || parsed.subnormal) {
        __detail::string_numeric_range("floating-point conversion out of range");
    }
    if (pos) *pos = static_cast<size_t>(parsed.end - begin);
    return rin_float_parse_binary32_value(parsed.bits);
}

inline wstring to_wstring(double value) {
    return __detail::narrow_to_wstring(to_string(value));
}

inline wstring to_wstring(float value) {
    return __detail::narrow_to_wstring(to_string(value));
}

/* ═══════════════════════════════════════════════════════════════
 * hash<string> 特殊化 - FNV-1a ハッシュ
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
struct hash;  /* 前方宣言 */

/* Match the standard's complete basic_string hash inventory.  An explicit
 * alias-only list silently disabled custom traits and allocator strings. */
template<typename CharT, typename Traits, typename Allocator>
struct hash<basic_string<CharT, Traits, Allocator>> {
    size_t operator()(
        const basic_string<CharT, Traits, Allocator>& str) const noexcept {
        return detail::libcxx_hash_character_sequence(str);
    }
};

/* ═══════════════════════════════════════════════════════════════
 * string_literals - user-defined string literals (C++14)
 * ═══════════════════════════════════════════════════════════════*/

inline namespace literals {
inline namespace string_literals {

#if __cplusplus >= 201402L
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#endif
inline string operator""s(const char* str, size_t len) {
    return string(str, len);
}

inline wstring operator""s(const wchar_t* str, size_t len) {
    return wstring(str, len);
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

/* string_view literals would be here if needed */

} /* namespace string_literals */
} /* namespace literals */

} /* namespace std */

/* See the include-order note on __detail::string_position_out_of_range. */
#include "exception.h"

/* Expose the C++17 view after basic_string itself is complete.  Including it
 * earlier recurses through exception.h while the view class is incomplete. */
#if __cplusplus >= 201703L
#include "string_view.h"
#if !defined(RINCXX_EXCEPTION_IN_PROGRESS) && \
    !defined(RINCXX_SYSTEM_ERROR_IN_PROGRESS)
#include "charconv.h"
#endif
#endif

/* C++17+ gets its representation-aware overload from charconv.h after that
 * header has finished its own declarations.  C++11/14 has no charconv owner,
 * so retain the bounded binary64 compatibility path here. */
#if __cplusplus < 201703L
namespace std {
inline string to_string(long double value) {
    return to_string(static_cast<double>(value));
}
inline wstring to_wstring(long double value) {
    return __detail::narrow_to_wstring(to_string(value));
}
} /* namespace std */
#endif

#endif /* __cplusplus */
#endif /* RINCXX_STRING_H */
