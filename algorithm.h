/*
 * RinOS C++ Algorithm ✿
 * algorithm 互換実装
 */

#ifndef RINCXX_ALGORITHM_H
#define RINCXX_ALGORITHM_H

#include "rincxx.h"
#include "iterator.h"
#include "initializer_list.h"
#include "utility.h"
#include "__random_bounded.h"
#if __cplusplus >= 201703L
#include "execution.h"
#endif

#ifdef __cplusplus

namespace std {

#if __cplusplus >= 201703L
namespace __detail {

template<typename Policy>
struct is_algorithm_execution_policy
    : is_execution_policy<remove_cv_t<remove_reference_t<Policy>>> {};

} /* namespace __detail */
#endif

#if __cplusplus >= 201402L
#define RIN_ALGORITHM_CONSTEXPR14 constexpr
#else
#define RIN_ALGORITHM_CONSTEXPR14 inline
#endif

/* ═══════════════════════════════════════════════════════════════
 * min / max
 * ═══════════════════════════════════════════════════════════════*/

template<typename T>
constexpr const T& min(const T& a, const T& b) {
    return (b < a) ? b : a;
}

template<typename T, typename Compare>
constexpr const T& min(const T& a, const T& b, Compare comp) {
    return comp(b, a) ? b : a;
}

template<typename T>
RIN_ALGORITHM_CONSTEXPR14 T min(initializer_list<T> ilist) {
    const T* first = ilist.begin();
    const T* last = ilist.end();
    const T* smallest = first;
    ++first;
    for (; first != last; ++first) {
        if (*first < *smallest) smallest = first;
    }
    return *smallest;
}

template<typename T>
constexpr const T& max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

template<typename T, typename Compare>
constexpr const T& max(const T& a, const T& b, Compare comp) {
    return comp(a, b) ? b : a;
}

template<typename T>
RIN_ALGORITHM_CONSTEXPR14 T max(initializer_list<T> ilist) {
    const T* first = ilist.begin();
    const T* last = ilist.end();
    const T* largest = first;
    ++first;
    for (; first != last; ++first) {
        if (*largest < *first) largest = first;
    }
    return *largest;
}

#if __cplusplus >= 201703L
template<typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}
#endif

/* ═══════════════════════════════════════════════════════════════
 * 非変更シーケンス操作
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename UnaryPredicate>
InputIt find_if(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (p(*first)) return first;
    }
    return last;
}

template<typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) return first;
    }
    return last;
}

template<typename InputIt, typename UnaryPredicate>
bool all_of(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (!p(*first)) return false;
    }
    return true;
}

template<typename InputIt, typename UnaryPredicate>
bool any_of(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (p(*first)) return true;
    }
    return false;
}

template<typename InputIt, typename UnaryPredicate>
bool none_of(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (p(*first)) return false;
    }
    return true;
}

template<typename InputIt, typename UnaryFunc>
UnaryFunc for_each(InputIt first, InputIt last, UnaryFunc f) {
    for (; first != last; ++first) {
        f(*first);
    }
    return f;
}

template<typename InputIt, typename T>
typename iterator_traits<InputIt>::difference_type
count(InputIt first, InputIt last, const T& value) {
    typename iterator_traits<InputIt>::difference_type ret = 0;
    for (; first != last; ++first) {
        if (*first == value) ++ret;
    }
    return ret;
}

template<typename InputIt, typename UnaryPredicate>
typename iterator_traits<InputIt>::difference_type
count_if(InputIt first, InputIt last, UnaryPredicate p) {
    typename iterator_traits<InputIt>::difference_type ret = 0;
    for (; first != last; ++first) {
        if (p(*first)) ++ret;
    }
    return ret;
}

/* equal - 2つの範囲が等しいか確認 */
template<typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
    for (; first1 != last1; ++first1, ++first2) {
        if (!(*first1 == *first2)) return false;
    }
    return true;
}

template<typename InputIt1, typename InputIt2, typename BinaryPredicate>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, BinaryPredicate p) {
    for (; first1 != last1; ++first1, ++first2) {
        if (!p(*first1, *first2)) return false;
    }
    return true;
}

/* equal with 4 iterators (C++14) */
template<typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!(*first1 == *first2)) return false;
    }
    return first1 == last1 && first2 == last2;
}

template<typename InputIt1, typename InputIt2, typename BinaryPredicate>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2,
           BinaryPredicate p) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!p(*first1, *first2)) return false;
    }
    return first1 == last1 && first2 == last2;
}

/* ═══════════════════════════════════════════════════════════════
 * 変更シーケンス操作
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt d_first) {
    while (first != last) {
        *d_first++ = *first++;
    }
    return d_first;
}

template<typename InputIt, typename OutputIt, typename UnaryPredicate>
OutputIt copy_if(InputIt first, InputIt last, OutputIt d_first, UnaryPredicate pred) {
    while (first != last) {
        if (pred(*first)) {
            *d_first++ = *first;
        }
        ++first;
    }
    return d_first;
}

template<typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value) {
    for (; first != last; ++first) {
        *first = value;
    }
}

template<typename OutputIt, typename Size, typename T>
OutputIt fill_n(OutputIt first, Size count, const T& value) {
    for (Size i = 0; i < count; ++i) {
        *first++ = value;
    }
    return first;
}

template<typename InputIt, typename OutputIt, typename UnaryOperation>
OutputIt transform(InputIt first, InputIt last, OutputIt d_first, UnaryOperation op) {
    while (first != last) {
        *d_first++ = op(*first++);
    }
    return d_first;
}

/* transform - 2入力範囲版（バイナリ演算） */
template<typename InputIt1, typename InputIt2, typename OutputIt, typename BinaryOperation>
OutputIt transform(InputIt1 first1, InputIt1 last1, InputIt2 first2, OutputIt d_first, BinaryOperation binary_op) {
    while (first1 != last1) {
        *d_first++ = binary_op(*first1++, *first2++);
    }
    return d_first;
}

template<typename ForwardIt, typename T>
void replace(ForwardIt first, ForwardIt last, const T& old_value, const T& new_value) {
    for (; first != last; ++first) {
        if (*first == old_value) {
            *first = new_value;
        }
    }
}

template<typename ForwardIt, typename T>
ForwardIt remove(ForwardIt first, ForwardIt last, const T& value) {
    first = find(first, last, value);
    if (first != last) {
        for (ForwardIt i = first; ++i != last;) {
            if (!(*i == value)) {
                *first++ = move(*i);
            }
        }
    }
    return first;
}

template<typename ForwardIt, typename UnaryPredicate>
ForwardIt remove_if(ForwardIt first, ForwardIt last, UnaryPredicate p) {
    first = find_if(first, last, p);
    if (first != last) {
        for (ForwardIt i = first; ++i != last;) {
            if (!p(*i)) {
                *first++ = move(*i);
            }
        }
    }
    return first;
}

template<typename ForwardIt>
ForwardIt unique(ForwardIt first, ForwardIt last) {
    if (first == last) return last;
    
    ForwardIt result = first;
    while (++first != last) {
        if (!(*result == *first) && ++result != first) {
            *result = move(*first);
        }
    }
    return ++result;
}

template<typename BidirIt>
void reverse(BidirIt first, BidirIt last) {
    while (first != last && first != --last) {
        swap(*first++, *last);
    }
}

/* rotate - 要素を回転（n_firstを新しい先頭に） */
template<typename ForwardIt>
ForwardIt rotate(ForwardIt first, ForwardIt n_first, ForwardIt last) {
    if (first == n_first) return last;
    if (n_first == last) return first;

    ForwardIt read = n_first;
    ForwardIt write = first;
    ForwardIt next_read = first;  // n_first の元の位置を覚えておく

    while (read != last) {
        if (write == next_read) {
            next_read = read;
        }
        swap(*write++, *read++);
    }

    // 残りの要素を回転
    rotate(write, next_read, last);
    return write;
}

/* ═══════════════════════════════════════════════════════════════
 * ソート - constexpr for C++20
 * ═══════════════════════════════════════════════════════════════*/

namespace __detail {

struct __algorithm_less {
    template<typename Left, typename Right>
    constexpr bool operator()(const Left& left, const Right& right) const {
        return left < right;
    }
};

template<typename BinaryPredicate, typename ForwardIt>
struct __algorithm_iterator_value_predicate {
    BinaryPredicate predicate;
    ForwardIt value_iterator;

    __algorithm_iterator_value_predicate(BinaryPredicate input_predicate,
                                         ForwardIt input_value_iterator)
        : predicate(input_predicate), value_iterator(input_value_iterator) {}

    template<typename Candidate>
    bool operator()(const Candidate& candidate) const {
        return predicate(candidate, *value_iterator);
    }
};

/* 挿入ソート（小規模向け） */
template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
    if (first == last) return;
    for (RandomIt i = first + 1; i != last; ++i) {
        auto key = std::move(*i);
        RandomIt j = i;
        while (j != first && comp(key, *(j - 1))) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(key);
    }
}

/* ヒープソート（constexpr対応、最悪O(n log n)） */
template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void heapify(
    RandomIt first,
    typename iterator_traits<RandomIt>::difference_type n,
    typename iterator_traits<RandomIt>::difference_type i,
    Compare comp) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    /* A recursive child index can be a valid leaf after a swap.  Keep the
     * entire sift-down iterative and guard each index before dereferencing;
     * this also lets GCC prove partial_sort_copy's short destination bound. */
    while (n >= 2 && i >= 0 && i < n) {
        const difference_type left = 2 * i + 1;
        if (left < 0 || left >= n) return;
        difference_type largest = i;
        if (comp(*(first + largest), *(first + left)))
            largest = left;
        const difference_type right = left + 1;
        if (right >= 0 && right < n &&
            comp(*(first + largest), *(first + right)))
            largest = right;
        if (largest == i) return;
        swap(*(first + i), *(first + largest));
        i = largest;
    }
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void heap_sort(RandomIt first, RandomIt last, Compare comp) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    difference_type n = last - first;
    if (n <= 1) return;

    // Build heap
    for (difference_type i = n / 2; i > 0; ) {
        --i;
        heapify(first, n, i, comp);
    }

    // Extract elements
    for (difference_type i = n - 1; i > 0; --i) {
        swap(*first, *(first + i));
        heapify(first, i, 0, comp);
    }
}

} // namespace __detail

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void sort(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    if (n <= 16) {
        __detail::insertion_sort(first, last, comp);
    } else {
        __detail::heap_sort(first, last, comp);
    }
}

template<typename RandomIt>
RIN_ALGORITHM_CONSTEXPR14 void sort(RandomIt first, RandomIt last) {
    sort(first, last, __detail::__algorithm_less());
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void stable_sort(RandomIt first, RandomIt last, Compare comp) {
    __detail::insertion_sort(first, last, comp);  /* 挿入ソートは安定 */
}

template<typename RandomIt>
RIN_ALGORITHM_CONSTEXPR14 void stable_sort(RandomIt first, RandomIt last) {
    stable_sort(first, last, __detail::__algorithm_less());
}

/* ═══════════════════════════════════════════════════════════════
 * バイナリサーチ
 * ═══════════════════════════════════════════════════════════════*/

template<typename ForwardIt, typename T>
ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value) {
    while (first != last) {
        ForwardIt mid = first;
        auto count = last - first;
        mid += count / 2;
        
        if (*mid < value) {
            first = mid;
            ++first;
        } else {
            last = mid;
        }
    }
    return first;
}

template<typename ForwardIt, typename T>
ForwardIt upper_bound(ForwardIt first, ForwardIt last, const T& value) {
    while (first != last) {
        ForwardIt mid = first;
        auto count = last - first;
        mid += count / 2;
        
        if (!(value < *mid)) {
            first = mid;
            ++first;
        } else {
            last = mid;
        }
    }
    return first;
}

/* lower_bound with custom comparator */
template<typename ForwardIt, typename T, typename Compare>
ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp) {
    while (first != last) {
        ForwardIt mid = first;
        auto count = last - first;
        mid += count / 2;

        if (comp(*mid, value)) {
            first = mid;
            ++first;
        } else {
            last = mid;
        }
    }
    return first;
}

/* upper_bound with custom comparator */
template<typename ForwardIt, typename T, typename Compare>
ForwardIt upper_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp) {
    while (first != last) {
        ForwardIt mid = first;
        auto count = last - first;
        mid += count / 2;

        if (!comp(value, *mid)) {
            first = mid;
            ++first;
        } else {
            last = mid;
        }
    }
    return first;
}

template<typename ForwardIt, typename T>
bool binary_search(ForwardIt first, ForwardIt last, const T& value) {
    first = lower_bound(first, last, value);
    return (first != last && !(value < *first));
}

template<typename ForwardIt, typename T, typename Compare>
bool binary_search(ForwardIt first, ForwardIt last, const T& value, Compare comp) {
    first = lower_bound(first, last, value, comp);
    return (first != last && !comp(value, *first));
}

/* ═══════════════════════════════════════════════════════════════
 * min/max要素
 * ═══════════════════════════════════════════════════════════════*/

template<typename ForwardIt>
ForwardIt min_element(ForwardIt first, ForwardIt last) {
    if (first == last) return last;
    
    ForwardIt smallest = first;
    while (++first != last) {
        if (*first < *smallest) {
            smallest = first;
        }
    }
    return smallest;
}

template<typename ForwardIt, typename Compare>
ForwardIt min_element(ForwardIt first, ForwardIt last, Compare comp) {
    if (first == last) return last;
    
    ForwardIt smallest = first;
    while (++first != last) {
        if (comp(*first, *smallest)) {
            smallest = first;
        }
    }
    return smallest;
}

template<typename ForwardIt>
ForwardIt max_element(ForwardIt first, ForwardIt last) {
    if (first == last) return last;
    
    ForwardIt largest = first;
    while (++first != last) {
        if (*largest < *first) {
            largest = first;
        }
    }
    return largest;
}

template<typename ForwardIt, typename Compare>
ForwardIt max_element(ForwardIt first, ForwardIt last, Compare comp) {
    if (first == last) return last;
    
    ForwardIt largest = first;
    while (++first != last) {
        if (comp(*largest, *first)) {
            largest = first;
        }
    }
    return largest;
}

/* accumulate は <numeric> に定義済み */

/* ═══════════════════════════════════════════════════════════════
 * 辞書順比較
 * ═══════════════════════════════════════════════════════════════*/

template<typename InputIt1, typename InputIt2>
bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
                             InputIt2 first2, InputIt2 last2) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (*first1 < *first2) return true;
        if (*first2 < *first1) return false;
    }
    return (first1 == last1) && (first2 != last2);
}

template<typename InputIt1, typename InputIt2, typename Compare>
bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
                             InputIt2 first2, InputIt2 last2,
                             Compare comp) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (comp(*first1, *first2)) return true;
        if (comp(*first2, *first1)) return false;
    }
    return (first1 == last1) && (first2 != last2);
}

/* mismatch */
template<typename InputIt1, typename InputIt2>
pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1,
                                       InputIt2 first2) {
    while (first1 != last1 && *first1 == *first2) {
        ++first1;
        ++first2;
    }
    return pair<InputIt1, InputIt2>(first1, first2);
}

template<typename InputIt1, typename InputIt2, typename BinaryPredicate>
pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1,
                                       InputIt2 first2, BinaryPredicate p) {
    while (first1 != last1 && p(*first1, *first2)) {
        ++first1;
        ++first2;
    }
    return pair<InputIt1, InputIt2>(first1, first2);
}

/* copy_n */
template<typename InputIt, typename Size, typename OutputIt>
OutputIt copy_n(InputIt first, Size count, OutputIt result) {
    for (Size i = 0; i < count; ++i) {
        *result++ = *first++;
    }
    return result;
}

/* move */
template<typename InputIt, typename OutputIt>
OutputIt move(InputIt first, InputIt last, OutputIt d_first) {
    while (first != last) {
        *d_first++ = move(*first++);
    }
    return d_first;
}

/* move_backward */
template<typename BidirIt1, typename BidirIt2>
BidirIt2 move_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last) {
    while (first != last) {
        *(--d_last) = move(*(--last));
    }
    return d_last;
}

/* copy_backward */
template<typename BidirIt1, typename BidirIt2>
BidirIt2 copy_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last) {
    while (first != last) {
        *(--d_last) = *(--last);
    }
    return d_last;
}

/* swap_ranges */
template<typename ForwardIt1, typename ForwardIt2>
ForwardIt2 swap_ranges(ForwardIt1 first1, ForwardIt1 last1, ForwardIt2 first2) {
    while (first1 != last1) {
        swap(*first1++, *first2++);
    }
    return first2;
}

/* iter_swap */
template<typename ForwardIt1, typename ForwardIt2>
void iter_swap(ForwardIt1 a, ForwardIt2 b) {
    swap(*a, *b);
}

/* ═══════════════════════════════════════════════════════════════
 * 追加アルゴリズム (Abseil互換)
 * ═══════════════════════════════════════════════════════════════*/

/* find_if_not */
template<typename InputIt, typename UnaryPredicate>
InputIt find_if_not(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (!p(*first)) return first;
    }
    return last;
}

/* search - find_endより前に定義 */
template<typename ForwardIt1, typename ForwardIt2>
ForwardIt1 search(ForwardIt1 first, ForwardIt1 last,
                  ForwardIt2 s_first, ForwardIt2 s_last) {
    for (;; ++first) {
        ForwardIt1 it = first;
        for (ForwardIt2 s_it = s_first;; ++it, ++s_it) {
            if (s_it == s_last) return first;
            if (it == last) return last;
            if (!(*it == *s_it)) break;
        }
    }
}

template<typename ForwardIt1, typename ForwardIt2, typename BinaryPredicate>
ForwardIt1 search(ForwardIt1 first, ForwardIt1 last,
                  ForwardIt2 s_first, ForwardIt2 s_last, BinaryPredicate p) {
    for (;; ++first) {
        ForwardIt1 it = first;
        for (ForwardIt2 s_it = s_first;; ++it, ++s_it) {
            if (s_it == s_last) return first;
            if (it == last) return last;
            if (!p(*it, *s_it)) break;
        }
    }
}

#if __cplusplus >= 201703L
/* C++17 searcher overload.  Searcher objects return the matching iterator
 * pair; the algorithm facade publishes the standard first iterator while
 * retaining the searcher's own predicate/hash state and bounds. */
template<typename ForwardIt, typename Searcher>
auto search(ForwardIt first, ForwardIt last, const Searcher& searcher)
    -> decltype(searcher(first, last).first) {
    return searcher(first, last).first;
}
#endif

/* find_end */
template<typename ForwardIt1, typename ForwardIt2>
ForwardIt1 find_end(ForwardIt1 first, ForwardIt1 last,
                    ForwardIt2 s_first, ForwardIt2 s_last) {
    if (s_first == s_last) return last;
    ForwardIt1 result = last;
    while (true) {
        ForwardIt1 new_result = search(first, last, s_first, s_last);
        if (new_result == last) return result;
        result = new_result;
        first = result;
        ++first;
    }
}

template<typename ForwardIt1, typename ForwardIt2, typename BinaryPredicate>
ForwardIt1 find_end(ForwardIt1 first, ForwardIt1 last,
                    ForwardIt2 s_first, ForwardIt2 s_last, BinaryPredicate p) {
    if (s_first == s_last) return last;
    ForwardIt1 result = last;
    while (true) {
        ForwardIt1 new_result = search(first, last, s_first, s_last, p);
        if (new_result == last) return result;
        result = new_result;
        first = result;
        ++first;
    }
}

/* find_first_of */
template<typename InputIt, typename ForwardIt>
InputIt find_first_of(InputIt first, InputIt last,
                      ForwardIt s_first, ForwardIt s_last) {
    for (; first != last; ++first) {
        for (ForwardIt it = s_first; it != s_last; ++it) {
            if (*first == *it) return first;
        }
    }
    return last;
}

template<typename InputIt, typename ForwardIt, typename BinaryPredicate>
InputIt find_first_of(InputIt first, InputIt last,
                      ForwardIt s_first, ForwardIt s_last, BinaryPredicate p) {
    for (; first != last; ++first) {
        for (ForwardIt it = s_first; it != s_last; ++it) {
            if (p(*first, *it)) return first;
        }
    }
    return last;
}

/* adjacent_find */
template<typename ForwardIt>
ForwardIt adjacent_find(ForwardIt first, ForwardIt last) {
    if (first == last) return last;
    ForwardIt next = first;
    ++next;
    for (; next != last; ++next, ++first) {
        if (*first == *next) return first;
    }
    return last;
}

template<typename ForwardIt, typename BinaryPredicate>
ForwardIt adjacent_find(ForwardIt first, ForwardIt last, BinaryPredicate p) {
    if (first == last) return last;
    ForwardIt next = first;
    ++next;
    for (; next != last; ++next, ++first) {
        if (p(*first, *next)) return first;
    }
    return last;
}

/* search_n */
template<typename ForwardIt, typename Size, typename T>
ForwardIt search_n(ForwardIt first, ForwardIt last, Size count, const T& value) {
    if (count <= 0) return first;
    for (; first != last; ++first) {
        if (*first == value) {
            ForwardIt candidate = first;
            Size cur_count = 1;
            while (true) {
                if (cur_count >= count) return candidate;
                ++first;
                if (first == last) return last;
                if (!(*first == value)) break;
                ++cur_count;
            }
        }
    }
    return last;
}

template<typename ForwardIt, typename Size, typename T, typename BinaryPredicate>
ForwardIt search_n(ForwardIt first, ForwardIt last, Size count, const T& value, BinaryPredicate p) {
    if (count <= 0) return first;
    for (; first != last; ++first) {
        if (p(*first, value)) {
            ForwardIt candidate = first;
            Size cur_count = 1;
            while (true) {
                if (cur_count >= count) return candidate;
                ++first;
                if (first == last) return last;
                if (!p(*first, value)) break;
                ++cur_count;
            }
        }
    }
    return last;
}

/* replace_if */
template<typename ForwardIt, typename UnaryPredicate, typename T>
void replace_if(ForwardIt first, ForwardIt last, UnaryPredicate p, const T& new_value) {
    for (; first != last; ++first) {
        if (p(*first)) {
            *first = new_value;
        }
    }
}

/* replace_copy */
template<typename InputIt, typename OutputIt, typename T>
OutputIt replace_copy(InputIt first, InputIt last, OutputIt d_first,
                      const T& old_value, const T& new_value) {
    for (; first != last; ++first) {
        *d_first++ = (*first == old_value) ? new_value : *first;
    }
    return d_first;
}

/* replace_copy_if */
template<typename InputIt, typename OutputIt, typename UnaryPredicate, typename T>
OutputIt replace_copy_if(InputIt first, InputIt last, OutputIt d_first,
                         UnaryPredicate p, const T& new_value) {
    for (; first != last; ++first) {
        *d_first++ = p(*first) ? new_value : *first;
    }
    return d_first;
}

/* generate */
template<typename ForwardIt, typename Generator>
void generate(ForwardIt first, ForwardIt last, Generator g) {
    for (; first != last; ++first) {
        *first = g();
    }
}

/* generate_n */
template<typename OutputIt, typename Size, typename Generator>
OutputIt generate_n(OutputIt first, Size count, Generator g) {
    for (Size i = 0; i < count; ++i) {
        *first++ = g();
    }
    return first;
}

/* remove_copy */
template<typename InputIt, typename OutputIt, typename T>
OutputIt remove_copy(InputIt first, InputIt last, OutputIt d_first, const T& value) {
    for (; first != last; ++first) {
        if (!(*first == value)) {
            *d_first++ = *first;
        }
    }
    return d_first;
}

/* remove_copy_if */
template<typename InputIt, typename OutputIt, typename UnaryPredicate>
OutputIt remove_copy_if(InputIt first, InputIt last, OutputIt d_first, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (!p(*first)) {
            *d_first++ = *first;
        }
    }
    return d_first;
}

/* unique_copy */
template<typename InputIt, typename OutputIt>
OutputIt unique_copy(InputIt first, InputIt last, OutputIt d_first) {
    if (first == last) return d_first;
    *d_first = *first;
    while (++first != last) {
        if (!(*d_first == *first)) {
            *++d_first = *first;
        }
    }
    return ++d_first;
}

template<typename InputIt, typename OutputIt, typename BinaryPredicate>
OutputIt unique_copy(InputIt first, InputIt last, OutputIt d_first, BinaryPredicate p) {
    if (first == last) return d_first;
    typename iterator_traits<InputIt>::value_type prev = *first;
    *d_first++ = prev;
    while (++first != last) {
        if (!p(prev, *first)) {
            prev = *first;
            *d_first++ = prev;
        }
    }
    return d_first;
}

/* reverse_copy */
template<typename BidirIt, typename OutputIt>
OutputIt reverse_copy(BidirIt first, BidirIt last, OutputIt d_first) {
    while (first != last) {
        *d_first++ = *--last;
    }
    return d_first;
}

/* rotate_copy */
template<typename ForwardIt, typename OutputIt>
OutputIt rotate_copy(ForwardIt first, ForwardIt n_first, ForwardIt last, OutputIt d_first) {
    d_first = copy(n_first, last, d_first);
    return copy(first, n_first, d_first);
}

/* shuffle */
template<typename RandomIt, typename URBG>
void shuffle(RandomIt first, RandomIt last, URBG&& g) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    using unsigned_difference =
        typename make_unsigned<difference_type>::type;

    const difference_type count = last - first;
    if (count <= 1) return;

    for (difference_type i = count - 1; i > 0; --i) {
        const unsigned long long bound = static_cast<unsigned long long>(
            static_cast<unsigned_difference>(i)) + 1u;
        const difference_type j = static_cast<difference_type>(
            __detail::__bounded_random(g, bound));
        swap(first[i], first[j]);
    }
}

namespace __detail {

template<typename Distance>
unsigned long long __sample_count(Distance count) {
    if (count <= static_cast<Distance>(0)) return 0u;
    using unsigned_distance = typename make_unsigned<Distance>::type;
    return static_cast<unsigned long long>(
        static_cast<unsigned_distance>(count));
}

template<typename PopulationIt, typename SampleIt, typename Distance,
         typename URBG>
SampleIt __sample_impl(PopulationIt first, PopulationIt last,
                       SampleIt out, Distance count, URBG& generator,
                       forward_iterator_tag) {
    using difference_type =
        typename iterator_traits<PopulationIt>::difference_type;
    using unsigned_difference =
        typename make_unsigned<difference_type>::type;

    const difference_type measured = std::distance(first, last);
    if (measured <= 0) return out;

    unsigned long long population = static_cast<unsigned long long>(
        static_cast<unsigned_difference>(measured));
    unsigned long long remaining = __sample_count(count);
    if (remaining > population) remaining = population;

    while (remaining != 0u) {
        if (remaining == population ||
            __bounded_random(generator, population) < remaining) {
            *out++ = *first;
            --remaining;
        }
        ++first;
        --population;
    }
    return out;
}

template<typename PopulationIt, typename SampleIt, typename Distance,
         typename URBG>
SampleIt __sample_impl(PopulationIt first, PopulationIt last,
                       SampleIt out, Distance count, URBG& generator,
                       input_iterator_tag) {
    const unsigned long long requested = __sample_count(count);
    if (requested == 0u) return out;

    SampleIt reservoir = out;
    unsigned long long filled = 0u;
    while (first != last && filled < requested) {
        *out++ = *first++;
        ++filled;
    }

    unsigned long long seen = filled;
    while (first != last) {
        const unsigned long long slot =
            __bounded_random(generator, seen + 1u);
        if (slot < filled) {
            using output_difference =
                typename iterator_traits<SampleIt>::difference_type;
            reservoir[static_cast<output_difference>(slot)] = *first;
        }
        ++first;
        ++seen;
    }
    return out;
}

} /* namespace __detail */

#if __cplusplus >= 201703L
/* sample */
template<typename PopulationIt, typename SampleIt, typename Distance, typename URBG>
SampleIt sample(PopulationIt first, PopulationIt last,
                SampleIt out, Distance n, URBG&& g) {
    return __detail::__sample_impl(
        first, last, out, n, g,
        typename iterator_traits<PopulationIt>::iterator_category());
}
#endif

/* is_partitioned */
template<typename InputIt, typename UnaryPredicate>
bool is_partitioned(InputIt first, InputIt last, UnaryPredicate p) {
    for (; first != last; ++first) {
        if (!p(*first)) break;
    }
    for (; first != last; ++first) {
        if (p(*first)) return false;
    }
    return true;
}

/* partition */
template<typename ForwardIt, typename UnaryPredicate>
ForwardIt partition(ForwardIt first, ForwardIt last, UnaryPredicate p) {
    first = find_if_not(first, last, p);
    if (first == last) return first;
    for (ForwardIt i = first; ++i != last;) {
        if (p(*i)) {
            swap(*first, *i);
            ++first;
        }
    }
    return first;
}

namespace __detail {

template<typename BidirIt, typename Distance, typename UnaryPredicate>
BidirIt stable_partition_without_buffer(BidirIt first, BidirIt last,
                                        Distance count,
                                        UnaryPredicate& predicate) {
    if (count == 0) return first;
    if (count == 1) return predicate(*first) ? last : first;

    Distance left_count = count / 2;
    BidirIt middle = first;
    advance(middle, left_count);

    BidirIt left_partition = stable_partition_without_buffer(
        first, middle, left_count, predicate);
    BidirIt right_partition = stable_partition_without_buffer(
        middle, last, count - left_count, predicate);

    return rotate(left_partition, middle, right_partition);
}

}  // namespace __detail

/* stable_partition - allocation-free divide/rotate implementation */
template<typename BidirIt, typename UnaryPredicate>
BidirIt stable_partition(BidirIt first, BidirIt last, UnaryPredicate predicate) {
    using difference_type =
        typename iterator_traits<BidirIt>::difference_type;
    difference_type count = distance(first, last);
    return __detail::stable_partition_without_buffer(
        first, last, count, predicate);
}

/* partition_copy */
template<typename InputIt, typename OutputIt1, typename OutputIt2, typename UnaryPredicate>
pair<OutputIt1, OutputIt2> partition_copy(InputIt first, InputIt last,
                                                OutputIt1 d_first_true,
                                                OutputIt2 d_first_false,
                                                UnaryPredicate p) {
    for (; first != last; ++first) {
        if (p(*first)) {
            *d_first_true++ = *first;
        } else {
            *d_first_false++ = *first;
        }
    }
    return pair<OutputIt1, OutputIt2>(d_first_true, d_first_false);
}

/* partition_point */
template<typename ForwardIt, typename UnaryPredicate>
ForwardIt partition_point(ForwardIt first, ForwardIt last, UnaryPredicate p) {
    auto n = last - first;
    while (n > 0) {
        auto half = n / 2;
        ForwardIt mid = first;
        mid += half;
        if (p(*mid)) {
            first = ++mid;
            n -= half + 1;
        } else {
            n = half;
        }
    }
    return first;
}

/* is_sorted */
template<typename ForwardIt>
bool is_sorted(ForwardIt first, ForwardIt last) {
    if (first == last) return true;
    ForwardIt next = first;
    while (++next != last) {
        if (*next < *first) return false;
        first = next;
    }
    return true;
}

template<typename ForwardIt, typename Compare>
bool is_sorted(ForwardIt first, ForwardIt last, Compare comp) {
    if (first == last) return true;
    ForwardIt next = first;
    while (++next != last) {
        if (comp(*next, *first)) return false;
        first = next;
    }
    return true;
}

/* is_sorted_until */
template<typename ForwardIt>
ForwardIt is_sorted_until(ForwardIt first, ForwardIt last) {
    if (first == last) return last;
    ForwardIt next = first;
    while (++next != last) {
        if (*next < *first) return next;
        first = next;
    }
    return last;
}

template<typename ForwardIt, typename Compare>
ForwardIt is_sorted_until(ForwardIt first, ForwardIt last, Compare comp) {
    if (first == last) return last;
    ForwardIt next = first;
    while (++next != last) {
        if (comp(*next, *first)) return next;
        first = next;
    }
    return last;
}

/* is_permutation */
template<typename ForwardIt1, typename ForwardIt2>
bool is_permutation(ForwardIt1 first1, ForwardIt1 last1, ForwardIt2 first2) {
    /* 長さが異なれば false */
    auto n = last1 - first1;
    ForwardIt2 last2 = first2;
    last2 += n;

    /* 先頭の一致部分をスキップ */
    while (first1 != last1 && *first1 == *first2) {
        ++first1;
        ++first2;
    }
    if (first1 == last1) return true;

    /* 残りの要素について順列かどうかチェック */
    for (ForwardIt1 it1 = first1; it1 != last1; ++it1) {
        /* この要素が既に数えられたかチェック */
        bool already_counted = false;
        for (ForwardIt1 it2 = first1; it2 != it1; ++it2) {
            if (*it2 == *it1) {
                already_counted = true;
                break;
            }
        }
        if (already_counted) continue;

        /* first1..last1 と first2..last2 でこの値の出現回数を数える */
        auto count1 = count(it1, last1, *it1);
        auto count2 = count(first2, last2, *it1);
        if (count1 != count2) return false;
    }
    return true;
}

template<typename ForwardIt1, typename ForwardIt2, typename BinaryPredicate>
bool is_permutation(ForwardIt1 first1, ForwardIt1 last1, ForwardIt2 first2, BinaryPredicate p) {
    auto n = last1 - first1;
    ForwardIt2 last2 = first2;
    last2 += n;

    while (first1 != last1 && p(*first1, *first2)) {
        ++first1;
        ++first2;
    }
    if (first1 == last1) return true;

    for (ForwardIt1 it1 = first1; it1 != last1; ++it1) {
        bool already_counted = false;
        for (ForwardIt1 it2 = first1; it2 != it1; ++it2) {
            if (p(*it2, *it1)) {
                already_counted = true;
                break;
            }
        }
        if (already_counted) continue;

        auto count1 = count_if(
            it1, last1,
            __detail::__algorithm_iterator_value_predicate<
                BinaryPredicate, ForwardIt1>(p, it1));
        auto count2 = count_if(
            first2, last2,
            __detail::__algorithm_iterator_value_predicate<
                BinaryPredicate, ForwardIt1>(p, it1));
        if (count1 != count2) return false;
    }
    return true;
}

/* next_permutation */
template<typename BidirIt>
bool next_permutation(BidirIt first, BidirIt last) {
    if (first == last) return false;
    BidirIt i = last;
    if (first == --i) return false;

    while (true) {
        BidirIt i1 = i;
        if (*--i < *i1) {
            BidirIt j = last;
            while (!(*i < *--j));
            swap(*i, *j);
            reverse(i1, last);
            return true;
        }
        if (i == first) {
            reverse(first, last);
            return false;
        }
    }
}

/* prev_permutation */
template<typename BidirIt>
bool prev_permutation(BidirIt first, BidirIt last) {
    if (first == last) return false;
    BidirIt i = last;
    if (first == --i) return false;

    while (true) {
        BidirIt i1 = i;
        if (*i1 < *--i) {
            BidirIt j = last;
            while (!(*--j < *i));
            swap(*i, *j);
            reverse(i1, last);
            return true;
        }
        if (i == first) {
            reverse(first, last);
            return false;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * ソート関連アルゴリズム
 * ═══════════════════════════════════════════════════════════════*/

namespace __detail {

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void partial_sort_impl(RandomIt first, RandomIt middle,
                                                  RandomIt last, Compare comp) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    const difference_type selected = middle - first;
    if (selected <= 0) return;

    for (difference_type index = selected / 2; index > 0; ) {
        --index;
        heapify(first, selected, index, comp);
    }

    for (RandomIt candidate = middle; candidate != last; ++candidate) {
        if (comp(*candidate, *first)) {
            swap(*candidate, *first);
            heapify(first, selected, 0, comp);
        }
    }
    heap_sort(first, middle, comp);
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 RandomIt median_iterator(RandomIt first, RandomIt middle,
                                                    RandomIt last, Compare comp) {
    if (comp(*first, *middle)) {
        if (comp(*middle, *last)) return middle;
        return comp(*first, *last) ? last : first;
    }
    if (comp(*first, *last)) return first;
    return comp(*middle, *last) ? last : middle;
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void nth_element_impl(RandomIt first, RandomIt nth,
                                                 RandomIt last, Compare comp) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    difference_type maximum_depth = 0;
    for (difference_type count = last - first; count > 1; count /= 2) {
        maximum_depth += 2;
    }

    difference_type depth = 0;
    while (last - first > 1) {
        if (depth++ > maximum_depth) {
            partial_sort_impl(first, nth + 1, last, comp);
            return;
        }

        const difference_type count = last - first;
        RandomIt pivot = median_iterator(
            first, first + count / 2, last - 1, comp);
        if (pivot != last - 1) swap(*pivot, *(last - 1));
        pivot = last - 1;

        RandomIt less_end = first;
        for (RandomIt current = first; current != pivot; ++current) {
            if (comp(*current, *pivot)) {
                if (current != less_end) swap(*current, *less_end);
                ++less_end;
            }
        }

        RandomIt equal_end = less_end;
        for (RandomIt current = less_end; current != pivot; ++current) {
            if (!comp(*pivot, *current)) {
                if (current != equal_end) swap(*current, *equal_end);
                ++equal_end;
            }
        }
        if (equal_end != pivot) swap(*equal_end, *pivot);
        ++equal_end;

        if (nth < less_end) {
            last = less_end;
        } else if (nth < equal_end) {
            return;
        } else {
            first = equal_end;
        }
    }
}

} /* namespace __detail */

/* partial_sort - bounded max-heap selection */
template<typename RandomIt>
RIN_ALGORITHM_CONSTEXPR14 void partial_sort(RandomIt first, RandomIt middle, RandomIt last) {
    __detail::partial_sort_impl(
        first, middle, last,
        __detail::__algorithm_less());
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void partial_sort(RandomIt first, RandomIt middle, RandomIt last,
                                             Compare comp) {
    __detail::partial_sort_impl(first, middle, last, comp);
}

/* partial_sort_copy */
template<typename InputIt, typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 RandomIt partial_sort_copy(InputIt first, InputIt last,
                                                      RandomIt d_first, RandomIt d_last,
                                                      Compare comp);

template<typename InputIt, typename RandomIt>
RIN_ALGORITHM_CONSTEXPR14 RandomIt partial_sort_copy(InputIt first, InputIt last,
                                                      RandomIt d_first, RandomIt d_last) {
    return partial_sort_copy(
        first, last, d_first, d_last,
        __detail::__algorithm_less());
}

template<typename InputIt, typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 RandomIt partial_sort_copy(InputIt first, InputIt last,
                                                      RandomIt d_first, RandomIt d_last,
                                                      Compare comp) {
    using difference_type =
        typename iterator_traits<RandomIt>::difference_type;
    RandomIt d_it = d_first;
    for (; first != last && d_it != d_last; ++first, ++d_it) {
        *d_it = *first;
    }
    const difference_type selected = d_it - d_first;
    if (selected <= 0) return d_it;

    for (difference_type index = selected / 2; index > 0; ) {
        --index;
        __detail::heapify(d_first, selected, index, comp);
    }

    for (; first != last; ++first) {
        if (comp(*first, *d_first)) {
            *d_first = *first;
            __detail::heapify(d_first, selected, 0, comp);
        }
    }
    __detail::heap_sort(d_first, d_it, comp);
    return d_it;
}

/* nth_element - three-way introselect */
template<typename RandomIt>
RIN_ALGORITHM_CONSTEXPR14 void nth_element(RandomIt first, RandomIt nth, RandomIt last) {
    if (first == last || nth == last) return;
    __detail::nth_element_impl(
        first, nth, last,
        __detail::__algorithm_less());
}

template<typename RandomIt, typename Compare>
RIN_ALGORITHM_CONSTEXPR14 void nth_element(RandomIt first, RandomIt nth, RandomIt last,
                                            Compare comp) {
    if (first == last || nth == last) return;
    __detail::nth_element_impl(first, nth, last, comp);
}

/* equal_range */
template<typename ForwardIt, typename T>
pair<ForwardIt, ForwardIt> equal_range(ForwardIt first, ForwardIt last, const T& value) {
    return pair<ForwardIt, ForwardIt>(lower_bound(first, last, value),
                                            upper_bound(first, last, value));
}

template<typename ForwardIt, typename T, typename Compare>
pair<ForwardIt, ForwardIt> equal_range(ForwardIt first, ForwardIt last,
                                             const T& value, Compare comp) {
    ForwardIt lo = first, hi = last;
    /* lower_bound with comp */
    while (lo != hi) {
        ForwardIt mid = lo;
        auto count = hi - lo;
        mid += count / 2;
        if (comp(*mid, value)) {
            lo = mid;
            ++lo;
        } else {
            hi = mid;
        }
    }
    ForwardIt lower = lo;

    /* upper_bound with comp */
    hi = last;
    while (lo != hi) {
        ForwardIt mid = lo;
        auto count = hi - lo;
        mid += count / 2;
        if (!comp(value, *mid)) {
            lo = mid;
            ++lo;
        } else {
            hi = mid;
        }
    }
    return pair<ForwardIt, ForwardIt>(lower, lo);
}

/* includes */
template<typename InputIt1, typename InputIt2>
bool includes(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
    for (; first2 != last2; ++first1) {
        if (first1 == last1 || *first2 < *first1) return false;
        if (!(*first1 < *first2)) ++first2;
    }
    return true;
}

template<typename InputIt1, typename InputIt2, typename Compare>
bool includes(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, Compare comp) {
    for (; first2 != last2; ++first1) {
        if (first1 == last1 || comp(*first2, *first1)) return false;
        if (!comp(*first1, *first2)) ++first2;
    }
    return true;
}

/* set_difference */
template<typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt set_difference(InputIt1 first1, InputIt1 last1,
                        InputIt2 first2, InputIt2 last2, OutputIt d_first) {
    while (first1 != last1) {
        if (first2 == last2) return copy(first1, last1, d_first);
        if (*first1 < *first2) {
            *d_first++ = *first1++;
        } else {
            if (!(*first2 < *first1)) ++first1;
            ++first2;
        }
    }
    return d_first;
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt set_difference(InputIt1 first1, InputIt1 last1,
                        InputIt2 first2, InputIt2 last2, OutputIt d_first,
                        Compare comp) {
    while (first1 != last1) {
        if (first2 == last2) return copy(first1, last1, d_first);
        if (comp(*first1, *first2)) {
            *d_first++ = *first1++;
        } else {
            if (!comp(*first2, *first1)) ++first1;
            ++first2;
        }
    }
    return d_first;
}

/* set_intersection */
template<typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt set_intersection(InputIt1 first1, InputIt1 last1,
                          InputIt2 first2, InputIt2 last2, OutputIt d_first) {
    while (first1 != last1 && first2 != last2) {
        if (*first1 < *first2) {
            ++first1;
        } else {
            if (!(*first2 < *first1)) {
                *d_first++ = *first1++;
            }
            ++first2;
        }
    }
    return d_first;
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt set_intersection(InputIt1 first1, InputIt1 last1,
                          InputIt2 first2, InputIt2 last2, OutputIt d_first,
                          Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            ++first1;
        } else {
            if (!comp(*first2, *first1)) *d_first++ = *first1++;
            ++first2;
        }
    }
    return d_first;
}

/* set_symmetric_difference */
template<typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt set_symmetric_difference(InputIt1 first1, InputIt1 last1,
                                  InputIt2 first2, InputIt2 last2, OutputIt d_first) {
    while (first1 != last1 && first2 != last2) {
        if (*first1 < *first2) {
            *d_first++ = *first1++;
        } else if (*first2 < *first1) {
            *d_first++ = *first2++;
        } else {
            ++first1;
            ++first2;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt set_symmetric_difference(InputIt1 first1, InputIt1 last1,
                                  InputIt2 first2, InputIt2 last2,
                                  OutputIt d_first, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            *d_first++ = *first1++;
        } else if (comp(*first2, *first1)) {
            *d_first++ = *first2++;
        } else {
            ++first1;
            ++first2;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

/* set_union */
template<typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt set_union(InputIt1 first1, InputIt1 last1,
                   InputIt2 first2, InputIt2 last2, OutputIt d_first) {
    while (first1 != last1 && first2 != last2) {
        if (*first1 < *first2) {
            *d_first++ = *first1++;
        } else if (*first2 < *first1) {
            *d_first++ = *first2++;
        } else {
            *d_first++ = *first1++;
            ++first2;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt set_union(InputIt1 first1, InputIt1 last1,
                   InputIt2 first2, InputIt2 last2, OutputIt d_first,
                   Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first1, *first2)) {
            *d_first++ = *first1++;
        } else if (comp(*first2, *first1)) {
            *d_first++ = *first2++;
        } else {
            *d_first++ = *first1++;
            ++first2;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

/* merge */
template<typename InputIt1, typename InputIt2, typename OutputIt>
OutputIt merge(InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2, OutputIt d_first) {
    while (first1 != last1 && first2 != last2) {
        if (*first2 < *first1) {
            *d_first++ = *first2++;
        } else {
            *d_first++ = *first1++;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

template<typename InputIt1, typename InputIt2, typename OutputIt, typename Compare>
OutputIt merge(InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2, OutputIt d_first, Compare comp) {
    while (first1 != last1 && first2 != last2) {
        if (comp(*first2, *first1)) {
            *d_first++ = *first2++;
        } else {
            *d_first++ = *first1++;
        }
    }
    d_first = copy(first1, last1, d_first);
    return copy(first2, last2, d_first);
}

namespace __detail {

template<typename ForwardIt, typename Distance, typename T, typename Compare>
ForwardIt lower_bound_count(ForwardIt first, Distance count,
                            const T& value, Compare comp) {
    while (count > 0) {
        const Distance step = count / 2;
        ForwardIt current = first;
        advance(current, step);
        if (comp(*current, value)) {
            first = current;
            ++first;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template<typename ForwardIt, typename Distance, typename T, typename Compare>
ForwardIt upper_bound_count(ForwardIt first, Distance count,
                            const T& value, Compare comp) {
    while (count > 0) {
        const Distance step = count / 2;
        ForwardIt current = first;
        advance(current, step);
        if (!comp(value, *current)) {
            first = current;
            ++first;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template<typename BidirIt, typename Distance, typename Compare>
void inplace_merge_without_buffer(BidirIt first, BidirIt middle, BidirIt last,
                                  Distance left_count, Distance right_count,
                                  Compare comp) {
    if (left_count == 0 || right_count == 0) return;
    if (left_count + right_count == 2) {
        if (comp(*middle, *first)) swap(*first, *middle);
        return;
    }

    BidirIt first_cut;
    BidirIt second_cut;
    Distance left_prefix;
    Distance right_prefix;
    if (left_count > right_count) {
        left_prefix = left_count / 2;
        first_cut = first;
        advance(first_cut, left_prefix);
        second_cut = lower_bound_count(
            middle, right_count, *first_cut, comp);
        right_prefix = distance(middle, second_cut);
    } else {
        right_prefix = right_count / 2;
        second_cut = middle;
        advance(second_cut, right_prefix);
        first_cut = upper_bound_count(
            first, left_count, *second_cut, comp);
        left_prefix = distance(first, first_cut);
    }

    BidirIt new_middle = rotate(first_cut, middle, second_cut);
    inplace_merge_without_buffer(
        first, first_cut, new_middle, left_prefix, right_prefix, comp);
    inplace_merge_without_buffer(
        new_middle, second_cut, last,
        left_count - left_prefix, right_count - right_prefix, comp);
}

} /* namespace __detail */

/* inplace_merge - allocation-free stable divide/rotate implementation */
template<typename BidirIt, typename Compare>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last, Compare comp);

template<typename BidirIt>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last) {
    inplace_merge(
        first, middle, last,
        __detail::__algorithm_less());
}

template<typename BidirIt, typename Compare>
void inplace_merge(BidirIt first, BidirIt middle, BidirIt last, Compare comp) {
    using difference_type =
        typename iterator_traits<BidirIt>::difference_type;
    const difference_type left_count = distance(first, middle);
    const difference_type right_count = distance(middle, last);
    if (left_count <= 0 || right_count <= 0) return;
    __detail::inplace_merge_without_buffer(
        first, middle, last, left_count, right_count, comp);
}

/* heap algorithms */
/* push_heap */
template<typename RandomIt>
void push_heap(RandomIt first, RandomIt last) {
    auto n = last - first;
    if (n < 2) return;
    auto hole = n - 1;
    auto value = move(*(first + hole));
    while (hole > 0) {
        auto parent = (hole - 1) / 2;
        if (*(first + parent) < value) {
            *(first + hole) = move(*(first + parent));
            hole = parent;
        } else {
            break;
        }
    }
    *(first + hole) = move(value);
}

template<typename RandomIt, typename Compare>
void push_heap(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    if (n < 2) return;
    auto hole = n - 1;
    auto value = move(*(first + hole));
    while (hole > 0) {
        auto parent = (hole - 1) / 2;
        if (comp(*(first + parent), value)) {
            *(first + hole) = move(*(first + parent));
            hole = parent;
        } else {
            break;
        }
    }
    *(first + hole) = move(value);
}

/* pop_heap */
template<typename RandomIt>
void pop_heap(RandomIt first, RandomIt last) {
    auto n = last - first;
    if (n < 2) return;
    swap(*first, *(last - 1));
    --last;
    n = last - first;
    if (n < 2) return;
    auto hole = decltype(n)(0);
    auto value = move(*first);
    while (true) {
        auto child = 2 * hole + 1;
        if (child >= n) break;
        if (child + 1 < n && *(first + child) < *(first + child + 1)) ++child;
        if (value < *(first + child)) {
            *(first + hole) = move(*(first + child));
            hole = child;
        } else {
            break;
        }
    }
    *(first + hole) = move(value);
}

template<typename RandomIt, typename Compare>
void pop_heap(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    if (n < 2) return;
    swap(*first, *(last - 1));
    --last;
    n = last - first;
    if (n < 2) return;
    auto hole = decltype(n)(0);
    auto value = move(*first);
    while (true) {
        auto child = 2 * hole + 1;
        if (child >= n) break;
        if (child + 1 < n && comp(*(first + child), *(first + child + 1))) ++child;
        if (comp(value, *(first + child))) {
            *(first + hole) = move(*(first + child));
            hole = child;
        } else {
            break;
        }
    }
    *(first + hole) = move(value);
}

/* make_heap */
template<typename RandomIt>
void make_heap(RandomIt first, RandomIt last) {
    auto n = last - first;
    if (n < 2) return;
    for (auto start = n / 2; start > 0; ) {
        --start;
        /* sift down */
        auto hole = start;
        auto value = move(*(first + hole));
        while (true) {
            auto child = 2 * hole + 1;
            if (child >= n) break;
            if (child + 1 < n && *(first + child) < *(first + child + 1)) ++child;
            if (value < *(first + child)) {
                *(first + hole) = move(*(first + child));
                hole = child;
            } else {
                break;
            }
        }
        *(first + hole) = move(value);
    }
}

template<typename RandomIt, typename Compare>
void make_heap(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    if (n < 2) return;
    for (auto start = n / 2; start > 0; ) {
        --start;
        auto hole = start;
        auto value = move(*(first + hole));
        while (true) {
            auto child = 2 * hole + 1;
            if (child >= n) break;
            if (child + 1 < n && comp(*(first + child), *(first + child + 1))) ++child;
            if (comp(value, *(first + child))) {
                *(first + hole) = move(*(first + child));
                hole = child;
            } else {
                break;
            }
        }
        *(first + hole) = move(value);
    }
}

/* sort_heap */
template<typename RandomIt>
void sort_heap(RandomIt first, RandomIt last) {
    while (first != last) {
        pop_heap(first, last);
        --last;
    }
}

template<typename RandomIt, typename Compare>
void sort_heap(RandomIt first, RandomIt last, Compare comp) {
    while (first != last) {
        pop_heap(first, last, comp);
        --last;
    }
}

/* is_heap */
template<typename RandomIt>
bool is_heap(RandomIt first, RandomIt last) {
    auto n = last - first;
    for (decltype(n) i = 1; i < n; ++i) {
        auto parent = (i - 1) / 2;
        if (*(first + parent) < *(first + i)) return false;
    }
    return true;
}

template<typename RandomIt, typename Compare>
bool is_heap(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    for (decltype(n) i = 1; i < n; ++i) {
        auto parent = (i - 1) / 2;
        if (comp(*(first + parent), *(first + i))) return false;
    }
    return true;
}

/* is_heap_until */
template<typename RandomIt>
RandomIt is_heap_until(RandomIt first, RandomIt last) {
    auto n = last - first;
    for (decltype(n) i = 1; i < n; ++i) {
        auto parent = (i - 1) / 2;
        if (*(first + parent) < *(first + i)) return first + i;
    }
    return last;
}

template<typename RandomIt, typename Compare>
RandomIt is_heap_until(RandomIt first, RandomIt last, Compare comp) {
    auto n = last - first;
    for (decltype(n) i = 1; i < n; ++i) {
        auto parent = (i - 1) / 2;
        if (comp(*(first + parent), *(first + i))) return first + i;
    }
    return last;
}

/* minmax */
template<typename T>
constexpr pair<const T&, const T&> minmax(const T& a, const T& b) {
    return (b < a) ? pair<const T&, const T&>(b, a)
                   : pair<const T&, const T&>(a, b);
}

template<typename T, typename Compare>
constexpr pair<const T&, const T&> minmax(const T& a, const T& b, Compare comp) {
    return comp(b, a) ? pair<const T&, const T&>(b, a)
                      : pair<const T&, const T&>(a, b);
}

/* minmax_element */
template<typename ForwardIt>
pair<ForwardIt, ForwardIt> minmax_element(ForwardIt first, ForwardIt last) {
    pair<ForwardIt, ForwardIt> result(first, first);
    if (first == last) return result;
    if (++first == last) return result;

    if (*first < *result.first) {
        result.first = first;
    } else {
        result.second = first;
    }

    while (++first != last) {
        ForwardIt next = first;
        if (++next == last) {
            if (*first < *result.first) {
                result.first = first;
            } else if (!(*first < *result.second)) {
                result.second = first;
            }
            break;
        }

        if (*next < *first) {
            if (*next < *result.first) result.first = next;
            if (!(*first < *result.second)) result.second = first;
        } else {
            if (*first < *result.first) result.first = first;
            if (!(*next < *result.second)) result.second = next;
        }
        first = next;
    }
    return result;
}

template<typename ForwardIt, typename Compare>
pair<ForwardIt, ForwardIt> minmax_element(ForwardIt first, ForwardIt last, Compare comp) {
    pair<ForwardIt, ForwardIt> result(first, first);
    if (first == last) return result;
    if (++first == last) return result;

    if (comp(*first, *result.first)) {
        result.first = first;
    } else {
        result.second = first;
    }

    while (++first != last) {
        ForwardIt next = first;
        if (++next == last) {
            if (comp(*first, *result.first)) {
                result.first = first;
            } else if (!comp(*first, *result.second)) {
                result.second = first;
            }
            break;
        }

        if (comp(*next, *first)) {
            if (comp(*next, *result.first)) result.first = next;
            if (!comp(*first, *result.second)) result.second = first;
        } else {
            if (comp(*first, *result.first)) result.first = first;
            if (!comp(*next, *result.second)) result.second = next;
        }
        first = next;
    }
    return result;
}

#if __cplusplus >= 201703L
/*
 * Standard execution-policy boundary and bounded worker dispatch. Elementwise
 * random-access ranges can use up to eight workers; small ranges, nested
 * dispatch, and non-random-access iterators stay on the caller. Unsequenced
 * policy loops carry compiler vectorization hints, while worker startup failure
 * degrades the affected chunk to caller execution.
 */

} /* namespace std */

namespace std {
class thread;
[[noreturn]] void terminate() noexcept;
} /* namespace std */

#include "atomic.h"
#include "exception.h"
#include "thread.h"

namespace std {
namespace __detail {

template<typename Policy>
struct is_parallel_algorithm_policy : false_type {};

template<>
struct is_parallel_algorithm_policy<execution::parallel_policy> : true_type {};

template<>
struct is_parallel_algorithm_policy<execution::parallel_unsequenced_policy>
    : true_type {};

template<typename Policy>
struct is_unsequenced_algorithm_policy : false_type {};

template<>
struct is_unsequenced_algorithm_policy<execution::parallel_unsequenced_policy>
    : true_type {};

#if __cplusplus >= 202002L
template<>
struct is_unsequenced_algorithm_policy<execution::unsequenced_policy>
    : true_type {};
#endif

/* A policy algorithm may be called recursively from an element operation.
 * Nested dispatch stays on the current worker so bounded per-call workers do
 * not multiply into an unbounded thread tree. */
inline unsigned& algorithm_worker_depth() noexcept {
    static thread_local unsigned depth = 0;
    return depth;
}

struct algorithm_worker_depth_guard {
    algorithm_worker_depth_guard() noexcept { ++algorithm_worker_depth(); }
    ~algorithm_worker_depth_guard() { --algorithm_worker_depth(); }
};

template<typename Callable, typename... Args>
inline bool try_start_algorithm_worker(thread& worker, Callable&& function,
                                      Args&&... args) {
    return std::detail::try_start_thread(
        worker, std::forward<Callable>(function),
        std::forward<Args>(args)...) == 0;
}

template<typename Worker, typename Callable, typename... Args>
inline auto try_start_algorithm_worker(Worker& worker, Callable&& function,
                                      Args&&... args)
    -> decltype(worker.try_start(std::forward<Callable>(function),
                                 std::forward<Args>(args)...)) {
    return worker.try_start(std::forward<Callable>(function),
                            std::forward<Args>(args)...);
}

template<typename Policy, typename Function>
inline auto invoke_algorithm_policy(Policy&&, Function&& function)
    -> decltype(std::forward<Function>(function)()) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
        return std::forward<Function>(function)();
    } catch (...) {
        /* All standard execution-policy overloads terminate when an
         * operation invoked by the algorithm lets an exception escape. */
        std::terminate();
    }
#else
    return std::forward<Function>(function)();
#endif
}

template<typename RandomIt, typename Difference, typename Function,
         typename Vectorize, typename Thread = thread>
inline void parallel_for_index(RandomIt first, Difference count,
                               Function function, false_type, Vectorize) {
    (void)first;
    if constexpr (Vectorize::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
        for (Difference index = 0; index < count; ++index)
            function(index);
    } else {
        for (Difference index = 0; index < count; ++index)
            function(index);
    }
}

template<typename RandomIt, typename Difference, typename Function,
         typename Vectorize, typename Thread = thread>
inline void parallel_for_index(RandomIt first, Difference count,
                               Function function, true_type, Vectorize) {
    (void)first;
    if (count <= 0) return;

    constexpr Difference minimum_parallel_range =
        static_cast<Difference>(2048);
    if (algorithm_worker_depth() != 0 || count < minimum_parallel_range) {
        if constexpr (Vectorize::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
            for (Difference index = 0; index < count; ++index)
                function(index);
        } else {
            for (Difference index = 0; index < count; ++index)
                function(index);
        }
        return;
    }

    unsigned int hardware_workers = Thread::hardware_concurrency();
    if (hardware_workers < 2u) {
        if constexpr (Vectorize::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
            for (Difference index = 0; index < count; ++index)
                function(index);
        } else {
            for (Difference index = 0; index < count; ++index)
                function(index);
        }
        return;
    }
    if (hardware_workers > 8u) hardware_workers = 8u;

    const Difference range_workers = count / minimum_parallel_range +
        (count % minimum_parallel_range != 0 ? 1 : 0);
    unsigned int worker_count = hardware_workers;
    if (static_cast<unsigned long long>(range_workers) < worker_count)
        worker_count = static_cast<unsigned int>(range_workers);
    if (worker_count < 2u) {
        if constexpr (Vectorize::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
            for (Difference index = 0; index < count; ++index)
                function(index);
        } else {
            for (Difference index = 0; index < count; ++index)
                function(index);
        }
        return;
    }

    const Difference worker_divisor = static_cast<Difference>(worker_count);
    const Difference base_chunk = count / worker_divisor;
    const Difference extra_items = count % worker_divisor;
    auto run_chunk = [function](Difference begin, Difference end) mutable {
        algorithm_worker_depth_guard depth_guard;
        if constexpr (Vectorize::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
            for (Difference index = begin; index < end; ++index)
                function(index);
        } else {
            for (Difference index = begin; index < end; ++index)
                function(index);
        }
    };

    Thread workers[7];
    unsigned int started = 0u;
    for (unsigned int worker = 1u; worker < worker_count; ++worker) {
        const Difference worker_index = static_cast<Difference>(worker);
        const Difference begin = worker_index * base_chunk +
            (worker_index < extra_items ? worker_index : extra_items);
        const Difference end = begin + base_chunk +
            (worker_index < extra_items ? 1 : 0);
        if (try_start_algorithm_worker(workers[started], run_chunk,
                                       begin, end))
            ++started;
        else
            run_chunk(begin, end);
    }

    const Difference main_end = base_chunk + (extra_items != 0 ? 1 : 0);
    run_chunk(0, main_end);
    for (unsigned int worker = 0u; worker < started; ++worker)
        workers[worker].join();
}

template<typename Function, typename Thread = thread>
inline void parallel_for_tasks(unsigned int task_count, Function function) {
    if (task_count == 0u) return;
    if (algorithm_worker_depth() != 0u || task_count == 1u) {
        for (unsigned int task = 0u; task < task_count; ++task)
            function(task);
        return;
    }

    const unsigned int dispatched_task_count =
        task_count > 8u ? 8u : task_count;

    auto run_task = [function](unsigned int task) mutable {
        algorithm_worker_depth_guard depth_guard;
        function(task);
    };
    Thread workers[7];
    unsigned int started = 0u;
    for (unsigned int task = 1u; task < dispatched_task_count; ++task) {
        if (try_start_algorithm_worker(workers[started], run_task, task))
            ++started;
        else
            run_task(task);
    }
    run_task(0u);
    for (unsigned int task = dispatched_task_count;
         task < task_count; ++task)
        run_task(task);
    for (unsigned int worker = 0u; worker < started; ++worker)
        workers[worker].join();
}



template<typename Policy, typename RandomIt, typename Difference,
         typename Function>
inline void policy_for_index(Policy&&, RandomIt first, Difference count,
                             Function function) {
    using policy_type = remove_cv_t<remove_reference_t<Policy>>;
    using should_parallelize = integral_constant<bool,
        is_parallel_algorithm_policy<policy_type>::value>;
    using should_vectorize = integral_constant<bool,
        is_unsequenced_algorithm_policy<policy_type>::value>;
    parallel_for_index(first, count,
                       [function](Difference index) mutable {
                           function(index);
                       }, should_parallelize{}, should_vectorize{});
}

template<typename Policy, typename RandomIt, typename Function>
inline void policy_for_each(Policy&& policy, RandomIt first, RandomIt last,
                            Function function, true_type) {
    using difference_type = typename iterator_traits<RandomIt>::difference_type;
    const difference_type count = last - first;
    policy_for_index(std::forward<Policy>(policy), first, count,
                     [first, function](difference_type index) mutable {
                         function(*(first + index));
                     });
}

template<typename Policy, typename InputIt, typename Function>
inline void policy_for_each(Policy&& policy, InputIt first, InputIt last,
                            Function function, false_type) {
    (void)policy;
    using policy_type = remove_cv_t<remove_reference_t<Policy>>;
    if constexpr (is_unsequenced_algorithm_policy<policy_type>::value) {
#if defined(__clang__)
#pragma clang loop vectorize(enable) interleave(enable)
#elif defined(__GNUC__)
#pragma GCC ivdep
#endif
        for (; first != last; ++first) function(*first);
    } else {
        for (; first != last; ++first) function(*first);
    }
}

template<typename Policy, typename InputIt, typename Function>
inline void policy_for_each(Policy&& policy, InputIt first, InputIt last,
                            Function function) {
    using category = typename iterator_traits<InputIt>::iterator_category;
    using random_access = is_base_of<random_access_iterator_tag, category>;
    policy_for_each(std::forward<Policy>(policy), first, last, function,
                    random_access{});
}


} /* namespace __detail */
} /* namespace std */

namespace std {

namespace __detail {

template<typename Policy, typename RandomIt, typename Compare,
         typename Thread = thread>
inline void policy_sort(Policy&& policy, RandomIt first, RandomIt last,
                       Compare compare, bool stable) {
    (void)policy;
    using policy_type = remove_cv_t<remove_reference_t<Policy>>;
    using difference_type = typename iterator_traits<RandomIt>::difference_type;
    const difference_type count = last - first;
    constexpr difference_type minimum_parallel_range =
        static_cast<difference_type>(4096);
    if (!is_parallel_algorithm_policy<policy_type>::value ||
        algorithm_worker_depth() != 0u || count < minimum_parallel_range) {
        if (stable) std::stable_sort(first, last, compare);
        else std::sort(first, last, compare);
        return;
    }

    unsigned int worker_count = Thread::hardware_concurrency();
    if (worker_count > 8u) worker_count = 8u;
    if (worker_count < 2u) {
        if (stable) std::stable_sort(first, last, compare);
        else std::sort(first, last, compare);
        return;
    }
    const difference_type range_workers =
        count / minimum_parallel_range +
        (count % minimum_parallel_range != 0 ? 1 : 0);
    if (static_cast<unsigned long long>(range_workers) < worker_count)
        worker_count = static_cast<unsigned int>(range_workers);
    if (worker_count < 2u) {
        if (stable) std::stable_sort(first, last, compare);
        else std::sort(first, last, compare);
        return;
    }

    const difference_type worker_divisor =
        static_cast<difference_type>(worker_count);
    const difference_type base_chunk = count / worker_divisor;
    const difference_type extra_items = count % worker_divisor;
    auto sort_task = [first, base_chunk, extra_items, compare, stable]
                     (unsigned int task) mutable {
            const difference_type task_index =
                static_cast<difference_type>(task);
            const difference_type begin = task_index * base_chunk +
                (task_index < extra_items ? task_index : extra_items);
            const difference_type end = begin + base_chunk +
                (task_index < extra_items ? 1 : 0);
            if (stable) std::stable_sort(first + begin, first + end, compare);
            else std::sort(first + begin, first + end, compare);
        };
    parallel_for_tasks<decltype(sort_task), Thread>(
        worker_count, std::move(sort_task));

    difference_type run_size = base_chunk + (extra_items != 0 ? 1 : 0);
    while (run_size < count) {
        const bool final_pass = run_size >= count - run_size;
        const difference_type combined_size =
            final_pass ? count : run_size * 2;
        for (difference_type begin = 0; begin < count;) {
            if (run_size >= count - begin) break;
            difference_type middle = begin + run_size;
            const difference_type remaining = count - begin;
            difference_type end = combined_size >= remaining
                ? count : begin + combined_size;
            std::inplace_merge(first + begin, first + middle, first + end,
                               compare);
            begin = end;
        }
        if (final_pass) break;
        run_size = combined_size;
    }
}

} /* namespace __detail */
template<typename ExecutionPolicy, typename InputIt, typename UnaryFunc,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void for_each(ExecutionPolicy&& policy, InputIt first, InputIt last,
              UnaryFunc function) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_for_each(policy, first, last, function);
        });
}

template<typename InputIt, typename Size, typename UnaryFunc>
InputIt for_each_n(InputIt first, Size count, UnaryFunc function) {
    for (Size i = 0; i < count; ++i, ++first)
        function(*first);
    return first;
}

template<typename ExecutionPolicy, typename InputIt, typename Size,
         typename UnaryFunc,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt for_each_n(ExecutionPolicy&& policy, InputIt first, Size count,
                   UnaryFunc function) {
    return __detail::invoke_algorithm_policy(policy, [&]() {
            InputIt last = first;
            for (Size index = 0; index < count; ++index) ++last;
            __detail::policy_for_each(policy, first, last, function);
            return last;
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool all_of(ExecutionPolicy&& policy, InputIt first, InputIt last,
            UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            atomic<bool> result(true);
            __detail::policy_for_each(policy, first, last,
                [predicate, &result](auto&& value) mutable {
                    if (!predicate(value)) result.store(false);
                });
            return result.load();
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool any_of(ExecutionPolicy&& policy, InputIt first, InputIt last,
            UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            atomic<bool> result(false);
            __detail::policy_for_each(policy, first, last,
                [predicate, &result](auto&& value) mutable {
                    if (predicate(value)) result.store(true);
                });
            return result.load();
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool none_of(ExecutionPolicy&& policy, InputIt first, InputIt last,
             UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            atomic<bool> result(true);
            __detail::policy_for_each(policy, first, last,
                [predicate, &result](auto&& value) mutable {
                    if (predicate(value)) result.store(false);
                });
            return result.load();
        });
}

template<typename ExecutionPolicy, typename InputIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt find(ExecutionPolicy&& policy, InputIt first, InputIt last,
             const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using category =
                typename iterator_traits<InputIt>::iterator_category;
            if constexpr (is_base_of<random_access_iterator_tag,
                                     category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                atomic<difference_type> match(count);
                __detail::policy_for_index(
                    policy, first, count,
                    [first, &match, &value](difference_type index) {
                        if (index >= match.load()) return;
                        if (*(first + index) != value) return;
                        difference_type current = match.load();
                        while (index < current &&
                               !match.compare_exchange_weak(current, index)) {}
                    });
                const difference_type index = match.load();
                return index < count ? first + index : last;
            } else {
                return find(first, last, value);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt find_if(ExecutionPolicy&& policy, InputIt first, InputIt last,
                UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using category =
                typename iterator_traits<InputIt>::iterator_category;
            if constexpr (is_base_of<random_access_iterator_tag,
                                     category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                atomic<difference_type> match(count);
                __detail::policy_for_index(
                    policy, first, count,
                    [first, &match, predicate](difference_type index) mutable {
                        if (index >= match.load() ||
                            !predicate(*(first + index))) return;
                        difference_type current = match.load();
                        while (index < current &&
                               !match.compare_exchange_weak(current, index)) {}
                    });
                const difference_type index = match.load();
                return index < count ? first + index : last;
            } else {
                return find_if(first, last, predicate);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt find_if_not(ExecutionPolicy&& policy, InputIt first, InputIt last,
                    UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using category =
                typename iterator_traits<InputIt>::iterator_category;
            if constexpr (is_base_of<random_access_iterator_tag,
                                     category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                atomic<difference_type> match(count);
                __detail::policy_for_index(
                    policy, first, count,
                    [first, &match, predicate](difference_type index) mutable {
                        if (index >= match.load() ||
                            predicate(*(first + index))) return;
                        difference_type current = match.load();
                        while (index < current &&
                               !match.compare_exchange_weak(current, index)) {}
                    });
                const difference_type index = match.load();
                return index < count ? first + index : last;
            } else {
                return find_if_not(first, last, predicate);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
typename iterator_traits<InputIt>::difference_type
count(ExecutionPolicy&& policy, InputIt first, InputIt last, const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            atomic<size_t> matches(0u);
            __detail::policy_for_each(policy, first, last,
                [&value, &matches](auto&& element) {
                    if (element == value) matches.fetch_add(1u);
                });
            return static_cast<
                typename iterator_traits<InputIt>::difference_type>(
                    matches.load());
        });
}

template<typename ExecutionPolicy, typename InputIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
typename iterator_traits<InputIt>::difference_type
count_if(ExecutionPolicy&& policy, InputIt first, InputIt last,
         UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            atomic<size_t> matches(0u);
            __detail::policy_for_each(policy, first, last,
                [predicate, &matches](auto&& element) mutable {
                    if (predicate(element)) matches.fetch_add(1u);
                });
            return static_cast<
                typename iterator_traits<InputIt>::difference_type>(
                    matches.load());
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt copy(ExecutionPolicy&& policy, InputIt first, InputIt last,
              OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result](difference_type index) {
                        *(result + index) = *(first + index);
                    });
                return result + count;
            } else {
                return copy(first, last, result);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt copy_if(ExecutionPolicy&& policy, InputIt first, InputIt last,
                 OutputIt result, UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return copy_if(first, last, result, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename Size,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt copy_n(ExecutionPolicy&& policy, InputIt first, Size count,
                OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                if (count <= 0) return result;
                const difference_type length =
                    static_cast<difference_type>(count);
                __detail::policy_for_index(
                    policy, first, length,
                    [first, result](difference_type index) {
                        *(result + index) = *(first + index);
                    });
                return result + length;
            } else {
                return copy_n(first, count, result);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt move(ExecutionPolicy&& policy, InputIt first, InputIt last,
              OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result](difference_type index) {
                        *(result + index) = std::move(*(first + index));
                    });
                return result + count;
            } else {
                return move(first, last, result);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename UnaryOperation,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt transform(ExecutionPolicy&& policy, InputIt first, InputIt last,
                   OutputIt result, UnaryOperation operation) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result, operation](difference_type index) mutable {
                        *(result + index) = operation(*(first + index));
                    });
                return result + count;
            } else {
                return transform(first, last, result, operation);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename BinaryOperation,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt transform(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
                   InputIt2 first2, OutputIt result,
                   BinaryOperation operation) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input1_category =
                typename iterator_traits<InputIt1>::iterator_category;
            using input2_category =
                typename iterator_traits<InputIt2>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input1_category>::value &&
                is_base_of<random_access_iterator_tag,
                           input2_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt1>::difference_type;
                const difference_type count = last1 - first1;
                __detail::policy_for_index(
                    policy, first1, count,
                    [first1, first2, result, operation]
                    (difference_type index) mutable {
                        *(result + index) = operation(
                            *(first1 + index), *(first2 + index));
                    });
                return result + count;
            } else {
                return transform(first1, last1, first2, result, operation);
            }
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void fill(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
          const T& value) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_for_each(policy, first, last,
                [&value](auto&& element) { element = value; });
        });
}

template<typename ExecutionPolicy, typename OutputIt, typename Size,
         typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt fill_n(ExecutionPolicy&& policy, OutputIt first, Size count,
                const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            OutputIt last = first;
            for (Size index = 0; index < count; ++index) ++last;
            __detail::policy_for_each(policy, first, last,
                [&value](auto&& element) { element = value; });
            return last;
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Generator,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void generate(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
              Generator generator) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_for_each(policy, first, last,
                [generator](auto&& element) mutable {
                    element = generator();
                });
        });
}

template<typename ExecutionPolicy, typename OutputIt, typename Size,
         typename Generator,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt generate_n(ExecutionPolicy&& policy, OutputIt first, Size count,
                    Generator generator) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            OutputIt last = first;
            for (Size index = 0; index < count; ++index) ++last;
            __detail::policy_for_each(policy, first, last,
                [generator](auto&& element) mutable {
                    element = generator();
                });
            return last;
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void replace(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
             const T& old_value, const T& new_value) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_for_each(policy, first, last,
                [&old_value, &new_value](auto&& element) {
                    if (element == old_value) element = new_value;
                });
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename UnaryPredicate,
         typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void replace_if(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
                UnaryPredicate predicate, const T& new_value) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_for_each(policy, first, last,
                [predicate, &new_value](auto&& element) mutable {
                    if (predicate(element)) element = new_value;
                });
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt replace_copy(ExecutionPolicy&& policy, InputIt first, InputIt last,
                      OutputIt result, const T& old_value,
                      const T& new_value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result, &old_value, &new_value]
                    (difference_type index) {
                        const auto& value = *(first + index);
                        *(result + index) = value == old_value
                            ? new_value : value;
                    });
                return result + count;
            } else {
                return replace_copy(first, last, result, old_value, new_value);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename UnaryPredicate, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt replace_copy_if(ExecutionPolicy&& policy, InputIt first,
                         InputIt last, OutputIt result,
                         UnaryPredicate predicate, const T& new_value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<InputIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<InputIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result, predicate, &new_value]
                    (difference_type index) mutable {
                        const auto& value = *(first + index);
                        *(result + index) = predicate(value)
                            ? new_value : value;
                    });
                return result + count;
            } else {
                return replace_copy_if(first, last, result, predicate,
                                       new_value);
            }
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt remove(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
                 const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return remove(first, last, value);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt remove_if(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
                    UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return remove_if(first, last, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt remove_copy(ExecutionPolicy&& policy, InputIt first, InputIt last,
                     OutputIt result, const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return remove_copy(first, last, result, value);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt remove_copy_if(ExecutionPolicy&& policy, InputIt first, InputIt last,
                        OutputIt result, UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return remove_copy_if(first, last, result, predicate);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt unique(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return unique(first, last);
        });
}

template<typename ExecutionPolicy, typename BidirIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void reverse(ExecutionPolicy&& policy, BidirIt first, BidirIt last) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            using category =
                typename iterator_traits<BidirIt>::iterator_category;
            if constexpr (is_base_of<random_access_iterator_tag,
                                     category>::value) {
                using difference_type =
                    typename iterator_traits<BidirIt>::difference_type;
                const difference_type count = last - first;
                const difference_type pairs = count / 2;
                __detail::policy_for_index(
                    policy, first, pairs,
                    [first, last](difference_type index) {
                        iter_swap(first + index, last - index - 1);
                    });
            } else {
                reverse(first, last);
            }
        });
}

template<typename ExecutionPolicy, typename BidirIt, typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt reverse_copy(ExecutionPolicy&& policy, BidirIt first, BidirIt last,
                      OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<BidirIt>::iterator_category;
            using output_category =
                typename iterator_traits<OutputIt>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<BidirIt>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, last, result](difference_type index) {
                        *(result + index) = *(last - index - 1);
                    });
                return result + count;
            } else {
                return reverse_copy(first, last, result);
            }
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt rotate(ExecutionPolicy&& policy, ForwardIt first,
                ForwardIt middle, ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return rotate(first, middle, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt rotate_copy(ExecutionPolicy&& policy, ForwardIt first,
                     ForwardIt middle, ForwardIt last, OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return rotate_copy(first, middle, last, result);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void sort(ExecutionPolicy&& policy, RandomIt first, RandomIt last) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_sort(policy, first, last,
                                  __detail::__algorithm_less(), false);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void sort(ExecutionPolicy&& policy, RandomIt first, RandomIt last,
          Compare compare) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_sort(policy, first, last, compare, false);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void stable_sort(ExecutionPolicy&& policy, RandomIt first, RandomIt last) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_sort(policy, first, last,
                                  __detail::__algorithm_less(), true);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void stable_sort(ExecutionPolicy&& policy, RandomIt first, RandomIt last,
                 Compare compare) {
    __detail::invoke_algorithm_policy(policy, [&]() {
            __detail::policy_sort(policy, first, last, compare, true);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void partial_sort(ExecutionPolicy&& policy, RandomIt first, RandomIt middle,
                  RandomIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            partial_sort(first, middle, last);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void partial_sort(ExecutionPolicy&& policy, RandomIt first, RandomIt middle,
                  RandomIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            partial_sort(first, middle, last, compare);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void nth_element(ExecutionPolicy&& policy, RandomIt first, RandomIt nth,
                 RandomIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            nth_element(first, nth, last);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void nth_element(ExecutionPolicy&& policy, RandomIt first, RandomIt nth,
                 RandomIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            nth_element(first, nth, last, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<InputIt1, InputIt2> mismatch(ExecutionPolicy&& policy, InputIt1 first1,
                                  InputIt1 last1, InputIt2 first2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return mismatch(first1, last1, first2);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<InputIt1, InputIt2> mismatch(ExecutionPolicy&& policy, InputIt1 first1,
                                  InputIt1 last1, InputIt2 first2,
                                  BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return mismatch(first1, last1, first2, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool equal(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
           InputIt2 first2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal(first1, last1, first2);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool equal(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
           InputIt2 first2, BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal(first1, last1, first2, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool equal(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
           InputIt2 first2, InputIt2 last2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal(first1, last1, first2, last2);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool equal(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
           InputIt2 first2, InputIt2 last2, BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal(first1, last1, first2, last2, predicate);
        });
}

template<typename ExecutionPolicy, typename BidirIt1, typename BidirIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
BidirIt2 copy_backward(ExecutionPolicy&& policy, BidirIt1 first,
                       BidirIt1 last, BidirIt2 result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return copy_backward(first, last, result);
        });
}

template<typename ExecutionPolicy, typename BidirIt1, typename BidirIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
BidirIt2 move_backward(ExecutionPolicy&& policy, BidirIt1 first,
                       BidirIt1 last, BidirIt2 result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return move_backward(first, last, result);
        });
}

template<typename ExecutionPolicy, typename ForwardIt1, typename ForwardIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt2 swap_ranges(ExecutionPolicy&& policy, ForwardIt1 first,
                       ForwardIt1 last, ForwardIt2 result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            using input_category =
                typename iterator_traits<ForwardIt1>::iterator_category;
            using output_category =
                typename iterator_traits<ForwardIt2>::iterator_category;
            if constexpr (
                is_base_of<random_access_iterator_tag,
                           input_category>::value &&
                is_base_of<random_access_iterator_tag,
                           output_category>::value) {
                using difference_type =
                    typename iterator_traits<ForwardIt1>::difference_type;
                const difference_type count = last - first;
                __detail::policy_for_index(
                    policy, first, count,
                    [first, result](difference_type index) {
                        iter_swap(first + index, result + index);
                    });
                return result + count;
            } else {
                return swap_ranges(first, last, result);
            }
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt unique_copy(ExecutionPolicy&& policy, InputIt first, InputIt last,
                     OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return unique_copy(first, last, result);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt unique_copy(ExecutionPolicy&& policy, InputIt first, InputIt last,
                     OutputIt result, BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return unique_copy(first, last, result, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename OutputIt1,
         typename OutputIt2, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<OutputIt1, OutputIt2> partition_copy(ExecutionPolicy&& policy,
                                           InputIt first, InputIt last,
                                           OutputIt1 true_result,
                                           OutputIt2 false_result,
                                           UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return partition_copy(first, last, true_result, false_result, predicate);
        });
}

template<typename ExecutionPolicy, typename BidirIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
BidirIt stable_partition(ExecutionPolicy&& policy, BidirIt first, BidirIt last,
                         UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return stable_partition(first, last, predicate);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename UnaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt partition_point(ExecutionPolicy&& policy, ForwardIt first,
                          ForwardIt last, UnaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return partition_point(first, last, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename SearchIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt search(ExecutionPolicy&& policy, InputIt first, InputIt last,
               SearchIt search_first, SearchIt search_last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return search(first, last, search_first, search_last);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename SearchIt,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt search(ExecutionPolicy&& policy, InputIt first, InputIt last,
               SearchIt search_first, SearchIt search_last,
               BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return search(first, last, search_first, search_last, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename Size,
         typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt search_n(ExecutionPolicy&& policy, InputIt first, InputIt last,
                 Size count, const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return search_n(first, last, count, value);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename Size,
         typename T, typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt search_n(ExecutionPolicy&& policy, InputIt first, InputIt last,
                 Size count, const T& value, BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return search_n(first, last, count, value, predicate);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename SearchIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt find_end(ExecutionPolicy&& policy, InputIt first, InputIt last,
                 SearchIt search_first, SearchIt search_last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return find_end(first, last, search_first, search_last);
        });
}

template<typename ExecutionPolicy, typename InputIt, typename SearchIt,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
InputIt find_end(ExecutionPolicy&& policy, InputIt first, InputIt last,
                 SearchIt search_first, SearchIt search_last,
                 BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return find_end(first, last, search_first, search_last, predicate);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt adjacent_find(ExecutionPolicy&& policy, ForwardIt first,
                        ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return adjacent_find(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt adjacent_find(ExecutionPolicy&& policy, ForwardIt first,
                        ForwardIt last, BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return adjacent_find(first, last, predicate);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt lower_bound(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return lower_bound(first, last, value);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt lower_bound(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, const T& value, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return lower_bound(first, last, value, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt upper_bound(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return upper_bound(first, last, value);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt upper_bound(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, const T& value, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return upper_bound(first, last, value, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool binary_search(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
                   const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return binary_search(first, last, value);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool binary_search(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
                   const T& value, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return binary_search(first, last, value, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<ForwardIt, ForwardIt> equal_range(ExecutionPolicy&& policy,
                                        ForwardIt first, ForwardIt last,
                                        const T& value) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal_range(first, last, value);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename T,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<ForwardIt, ForwardIt> equal_range(ExecutionPolicy&& policy,
                                        ForwardIt first, ForwardIt last,
                                        const T& value, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return equal_range(first, last, value, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_sorted(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_sorted(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_sorted(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last,
               Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_sorted(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt is_sorted_until(ExecutionPolicy&& policy, ForwardIt first,
                          ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_sorted_until(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt is_sorted_until(ExecutionPolicy&& policy, ForwardIt first,
                          ForwardIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_sorted_until(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_heap(ExecutionPolicy&& policy, RandomIt first, RandomIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_heap(first, last);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_heap(ExecutionPolicy&& policy, RandomIt first, RandomIt last,
             Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_heap(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename RandomIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
RandomIt is_heap_until(ExecutionPolicy&& policy, RandomIt first,
                       RandomIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_heap_until(first, last);
        });
}

template<typename ExecutionPolicy, typename RandomIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
RandomIt is_heap_until(ExecutionPolicy&& policy, RandomIt first,
                       RandomIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_heap_until(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool includes(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
              InputIt2 first2, InputIt2 last2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return includes(first1, last1, first2, last2);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool includes(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
              InputIt2 first2, InputIt2 last2, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return includes(first1, last1, first2, last2, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_difference(ExecutionPolicy&& policy, InputIt1 first1,
                        InputIt1 last1, InputIt2 first2, InputIt2 last2,
                        OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_difference(first1, last1, first2, last2, result);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_difference(ExecutionPolicy&& policy, InputIt1 first1,
                        InputIt1 last1, InputIt2 first2, InputIt2 last2,
                        OutputIt result, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_difference(first1, last1, first2, last2, result, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_intersection(ExecutionPolicy&& policy, InputIt1 first1,
                          InputIt1 last1, InputIt2 first2, InputIt2 last2,
                          OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_intersection(first1, last1, first2, last2, result);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_intersection(ExecutionPolicy&& policy, InputIt1 first1,
                          InputIt1 last1, InputIt2 first2, InputIt2 last2,
                          OutputIt result, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_intersection(first1, last1, first2, last2, result, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_symmetric_difference(ExecutionPolicy&& policy, InputIt1 first1,
                                  InputIt1 last1, InputIt2 first2,
                                  InputIt2 last2, OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_symmetric_difference(first1, last1, first2, last2, result);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_symmetric_difference(ExecutionPolicy&& policy, InputIt1 first1,
                                  InputIt1 last1, InputIt2 first2,
                                  InputIt2 last2, OutputIt result,
                                  Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_symmetric_difference(first1, last1, first2, last2, result,
                                    compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_union(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
                   InputIt2 first2, InputIt2 last2, OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_union(first1, last1, first2, last2, result);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt set_union(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
                   InputIt2 first2, InputIt2 last2, OutputIt result,
                   Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return set_union(first1, last1, first2, last2, result, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt merge(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2, OutputIt result) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return merge(first1, last1, first2, last2, result);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename OutputIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
OutputIt merge(ExecutionPolicy&& policy, InputIt1 first1, InputIt1 last1,
               InputIt2 first2, InputIt2 last2, OutputIt result,
               Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return merge(first1, last1, first2, last2, result, compare);
        });
}

template<typename ExecutionPolicy, typename BidirIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void inplace_merge(ExecutionPolicy&& policy, BidirIt first, BidirIt middle,
                   BidirIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            inplace_merge(first, middle, last);
        });
}

template<typename ExecutionPolicy, typename BidirIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
void inplace_merge(ExecutionPolicy&& policy, BidirIt first, BidirIt middle,
                   BidirIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            inplace_merge(first, middle, last, compare);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool lexicographical_compare(ExecutionPolicy&& policy, InputIt1 first1,
                             InputIt1 last1, InputIt2 first2,
                             InputIt2 last2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return lexicographical_compare(first1, last1, first2, last2);
        });
}

template<typename ExecutionPolicy, typename InputIt1, typename InputIt2,
         typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool lexicographical_compare(ExecutionPolicy&& policy, InputIt1 first1,
                             InputIt1 last1, InputIt2 first2,
                             InputIt2 last2, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return lexicographical_compare(first1, last1, first2, last2, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt1, typename ForwardIt2,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_permutation(ExecutionPolicy&& policy, ForwardIt1 first1,
                    ForwardIt1 last1, ForwardIt2 first2) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_permutation(first1, last1, first2);
        });
}

template<typename ExecutionPolicy, typename ForwardIt1, typename ForwardIt2,
         typename BinaryPredicate,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
bool is_permutation(ExecutionPolicy&& policy, ForwardIt1 first1,
                    ForwardIt1 last1, ForwardIt2 first2,
                    BinaryPredicate predicate) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return is_permutation(first1, last1, first2, predicate);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt min_element(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return min_element(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt min_element(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return min_element(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt max_element(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return max_element(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
ForwardIt max_element(ExecutionPolicy&& policy, ForwardIt first,
                      ForwardIt last, Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return max_element(first, last, compare);
        });
}

template<typename ExecutionPolicy, typename ForwardIt,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<ForwardIt, ForwardIt> minmax_element(ExecutionPolicy&& policy,
                                           ForwardIt first, ForwardIt last) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return minmax_element(first, last);
        });
}

template<typename ExecutionPolicy, typename ForwardIt, typename Compare,
         enable_if_t<__detail::is_algorithm_execution_policy<
                          ExecutionPolicy>::value, int> = 0>
pair<ForwardIt, ForwardIt> minmax_element(ExecutionPolicy&& policy,
                                           ForwardIt first, ForwardIt last,
                                           Compare compare) {
    return __detail::invoke_algorithm_policy(
        policy, [&]() -> decltype(auto) {
            return minmax_element(first, last, compare);
        });
}
#endif

#undef RIN_ALGORITHM_CONSTEXPR14

} /* namespace std */

/* Common algorithm functions in global namespace for C compatibility */
using std::transform;
using std::for_each;
using std::find;
using std::find_if;
using std::count;
using std::count_if;
using std::copy;
using std::copy_n;
using std::fill;
using std::fill_n;
using std::swap;
using std::sort;
using std::stable_sort;

/* iterator_traitsはiterator.hで定義済み */

#endif /* __cplusplus */
#endif /* RINCXX_ALGORITHM_H */
