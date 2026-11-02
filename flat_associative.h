/*
 * RinOS C++23 flat associative containers.
 *
 * The containers keep their ordering in caller-selectable random-access
 * sequence containers.  Mutations are logarithmic to locate and linear to
 * shift, while lookup walks contiguous storage.
 */
#ifndef RINCXX_FLAT_ASSOCIATIVE_H
#define RINCXX_FLAT_ASSOCIATIVE_H

#include "algorithm.h"
#include "compare.h"
#include "functional.h"
#include "initializer_list.h"
#include "iterator.h"
#include "memory.h"
#include "rincxx.h"
#include "type_traits.h"
#include "utility.h"
#include "vector.h"
#if __cplusplus > 202002L
#include "ranges.h"
#endif

#if __cplusplus > 202002L
namespace std {

struct sorted_unique_t {
    explicit constexpr sorted_unique_t() = default;
};
struct sorted_equivalent_t {
    explicit constexpr sorted_equivalent_t() = default;
};
inline constexpr sorted_unique_t sorted_unique{};
inline constexpr sorted_equivalent_t sorted_equivalent{};

namespace flat_detail {

[[noreturn]] inline void at_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw out_of_range("flat_map::at");
#else
    __builtin_trap();
#endif
}

template <typename C> auto iter_at(C &values, size_t index) -> decltype(values.begin()) {
    return values.empty() ? values.begin() : values.begin() + index;
}
template <typename C> auto iter_at(const C &values, size_t index) -> decltype(values.begin()) {
    return values.empty() ? values.begin() : values.begin() + index;
}

template <typename C, typename K, typename Compare>
size_t lower_index(const C &values, const K &key, const Compare &comp) {
    size_t first = 0;
    size_t count = values.size();
    while (count != 0) {
        const size_t step = count / 2;
        const size_t middle = first + step;
        if (comp(values[middle], key)) {
            first = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename C, typename K, typename Compare>
size_t upper_index(const C &values, const K &key, const Compare &comp) {
    size_t first = 0;
    size_t count = values.size();
    while (count != 0) {
        const size_t step = count / 2;
        const size_t middle = first + step;
        if (!comp(key, values[middle])) {
            first = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first;
}

template <typename C, typename Alloc> C make_container(const Alloc &alloc) { return C(alloc); }

template <typename C> C make_container() { return C(); }

template <typename C, typename I> C copy_container(I first, I last) {
    C result;
    result.insert(result.end(), first, last);
    return result;
}

template <typename C, typename I, typename Alloc>
C copy_container(I first, I last, const Alloc &alloc) {
    C result(alloc);
    result.insert(result.end(), first, last);
    return result;
}

template <typename C, typename Alloc>
typename C::allocator_type sequence_allocator(const Alloc &alloc) {
    return typename C::allocator_type(alloc);
}

template <typename Value, typename It1, typename It2>
constexpr detail::synth_three_way_result_t<Value> compare_sequences(It1 first1, It1 last1,
                                                                    It2 first2, It2 last2) {
    using result_type = detail::synth_three_way_result_t<Value>;
    while (first1 != last1 && first2 != last2) {
        const auto order = detail::synth_three_way(*first1, *first2);
        if (order < 0)
            return result_type::less;
        if (order > 0)
            return result_type::greater;
        if (order != 0)
            return static_cast<result_type>(order);
        ++first1;
        ++first2;
    }
    if (first1 == last1 && first2 == last2)
        return result_type::equivalent;
    return first1 == last1 ? result_type::less : result_type::greater;
}

template <typename Pair>
using pair_key_t = remove_cv_t<remove_reference_t<decltype(declval<Pair>().first)>>;
template <typename Pair>
using pair_mapped_t = remove_cv_t<remove_reference_t<decltype(declval<Pair>().second)>>;

} // namespace flat_detail

template <typename Key, typename Compare = less<Key>, typename KeyContainer = vector<Key>>
class flat_set {
  public:
    using key_type = Key;
    using value_type = Key;
    using container_type = KeyContainer;
    using key_compare = Compare;
    using value_compare = Compare;
    using key_container_type = KeyContainer;
    using allocator_type = typename KeyContainer::allocator_type;
    using size_type = typename KeyContainer::size_type;
    using difference_type = typename KeyContainer::difference_type;
    using reference = const value_type &;
    using const_reference = const value_type &;
    using const_iterator = typename KeyContainer::const_iterator;
    using iterator = const_iterator;
    using const_reverse_iterator = reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    static_assert(is_same<typename KeyContainer::value_type, Key>::value,
                  "flat_set Key must match KeyContainer::value_type");

  private:
    Compare comp_;
    KeyContainer keys_;

    template <typename K> size_type lower(const K &key) const {
        return static_cast<size_type>(flat_detail::lower_index(keys_, key, comp_));
    }
    template <typename K> size_type upper(const K &key) const {
        return static_cast<size_type>(flat_detail::upper_index(keys_, key, comp_));
    }
    template <typename K> pair<iterator, bool> insert_value(K &&key) {
        const size_type index = lower(key);
        if (index != size() && !comp_(key, keys_[index]))
            return pair<iterator, bool>(flat_detail::iter_at(keys_, index), false);
        iterator pos = keys_.insert(flat_detail::iter_at(keys_, index), std::forward<K>(key));
        return pair<iterator, bool>(pos, true);
    }
    void sort_unique() {
        sort(keys_.begin(), keys_.end(), comp_);
        auto out = keys_.begin();
        for (auto it = keys_.begin(); it != keys_.end(); ++it) {
            if (out == keys_.begin() || comp_(*(out - 1), *it)) {
                if (out != it)
                    *out = std::move(*it);
                ++out;
            }
        }
        keys_.erase(out, keys_.end());
    }

  protected:
    template <typename K> iterator insert_equivalent(K &&key) {
        const size_type index = upper(key);
        return keys_.insert(flat_detail::iter_at(keys_, index), std::forward<K>(key));
    }
    template <typename InputIt> void insert_equivalent_range(InputIt first, InputIt last) {
        for (; first != last; ++first)
            insert_equivalent(*first);
    }
    KeyContainer &mutable_key_container() noexcept { return keys_; }

  public:
    flat_set() : comp_(), keys_() {}
    explicit flat_set(const Compare &comp) : comp_(comp), keys_() {}
    flat_set(const flat_set &) = default;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    flat_set(flat_set &&other) try : comp_(std::move(other.comp_)), keys_(std::move(other.keys_)) {
        other.clear();
    } catch (...) {
        other.clear();
        throw;
    }
#else
    flat_set(flat_set &&other) : comp_(std::move(other.comp_)), keys_(std::move(other.keys_)) {
        other.clear();
    }
#endif
    flat_set &operator=(const flat_set &other) {
        if (this != &other) {
            flat_set candidate(other);
            swap(candidate);
        }
        return *this;
    }
    flat_set &operator=(flat_set &&other) {
        if (this != &other) {
            flat_set candidate(std::move(other));
            swap(candidate);
        }
        return *this;
    }
    explicit flat_set(const allocator_type &alloc) : comp_(), keys_(alloc) {}
    flat_set(const Compare &comp, const allocator_type &alloc) : comp_(comp), keys_(alloc) {}
    flat_set(const flat_set &other, const allocator_type &alloc)
        : comp_(other.comp_), keys_(flat_detail::copy_container<KeyContainer>(
                                  other.keys_.begin(), other.keys_.end(), alloc)) {}
    flat_set(flat_set &&other, const allocator_type &alloc)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try
#endif
        : comp_(std::move(other.comp_)), keys_(flat_detail::copy_container<KeyContainer>(
                                             std::make_move_iterator(other.keys_.begin()),
                                             std::make_move_iterator(other.keys_.end()), alloc)) {
        other.clear();
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    catch (...) {
        other.clear();
        throw;
    }
#endif
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    explicit flat_set(const Alloc &alloc)
        : comp_(), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(const flat_set &other, const Alloc &alloc)
        : comp_(other.comp_), keys_(flat_detail::copy_container<KeyContainer>(
                                  other.keys_.begin(), other.keys_.end(),
                                  flat_detail::sequence_allocator<KeyContainer>(alloc))) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(flat_set &&other, const Alloc &alloc)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try
#endif
        : comp_(std::move(other.comp_)),
          keys_(flat_detail::copy_container<KeyContainer>(
              std::make_move_iterator(other.keys_.begin()),
              std::make_move_iterator(other.keys_.end()),
              flat_detail::sequence_allocator<KeyContainer>(alloc))) {
        other.clear();
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    catch (...) {
        other.clear();
        throw;
    }
#endif
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(const KeyContainer &values, const Alloc &alloc)
        : comp_(),
          keys_(flat_detail::copy_container<KeyContainer>(
              values.begin(), values.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))) {
        sort_unique();
    }
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(const KeyContainer &values, const Compare &comp, const Alloc &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(
              values.begin(), values.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))) {
        sort_unique();
    }
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, const KeyContainer &values, const Alloc &alloc)
        : flat_set(sorted_unique, values, Compare(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, const KeyContainer &values, const Compare &comp, const Alloc &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(
              values.begin(), values.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))) {
    }
    explicit flat_set(KeyContainer values, const Compare &comp = Compare())
        : comp_(comp), keys_(std::move(values)) {
        sort_unique();
    }
    flat_set(const KeyContainer &values, const allocator_type &alloc)
        : comp_(),
          keys_(flat_detail::copy_container<KeyContainer>(values.begin(), values.end(), alloc)) {
        sort_unique();
    }
    flat_set(const KeyContainer &values, const Compare &comp, const allocator_type &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(values.begin(), values.end(), alloc)) {
        sort_unique();
    }
    flat_set(sorted_unique_t, const KeyContainer &values, const Compare &comp = Compare())
        : comp_(comp), keys_(values) {}
    flat_set(sorted_unique_t, KeyContainer &&values, const Compare &comp = Compare())
        : comp_(comp), keys_(std::move(values)) {}
    flat_set(sorted_unique_t, const KeyContainer &values, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(values.begin(), values.end(), alloc)) {}
    flat_set(sorted_unique_t, const KeyContainer &values, const allocator_type &alloc)
        : flat_set(sorted_unique, values, Compare(), alloc) {}
    flat_set(sorted_unique_t, KeyContainer &&values, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(std::make_move_iterator(values.begin()),
                                                          std::make_move_iterator(values.end()),
                                                          alloc)) {}

    template <typename InputIt>
    flat_set(InputIt first, InputIt last, const Compare &comp = Compare()) : comp_(comp), keys_() {
        insert(first, last);
    }
    template <typename InputIt>
    flat_set(InputIt first, InputIt last, const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(alloc) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_set(InputIt first, InputIt last, const allocator_type &alloc)
        : flat_set(first, last, Compare(), alloc) {}
    template <typename InputIt>
    flat_set(sorted_unique_t, InputIt first, InputIt last, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp), keys_(alloc) {
        keys_.insert(keys_.end(), first, last);
    }
    template <typename InputIt>
    flat_set(sorted_unique_t, InputIt first, InputIt last, const allocator_type &alloc)
        : flat_set(sorted_unique, first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(InputIt first, InputIt last, const Alloc &alloc)
        : flat_set(first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(InputIt first, InputIt last, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)) {
        keys_.insert(keys_.end(), first, last);
        sort_unique();
    }
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, InputIt first, InputIt last, const Alloc &alloc)
        : flat_set(sorted_unique, first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, InputIt first, InputIt last, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)) {
        keys_.insert(keys_.end(), first, last);
    }
    template <typename InputIt>
    flat_set(sorted_unique_t, InputIt first, InputIt last, const Compare &comp = Compare())
        : comp_(comp), keys_() {
        keys_.insert(keys_.end(), first, last);
    }
    flat_set(initializer_list<value_type> values, const Compare &comp = Compare())
        : flat_set(values.begin(), values.end(), comp) {}
    flat_set(initializer_list<value_type> values, const Compare &comp, const allocator_type &alloc)
        : flat_set(values.begin(), values.end(), comp, alloc) {}
    flat_set(initializer_list<value_type> values, const allocator_type &alloc)
        : flat_set(values.begin(), values.end(), Compare(), alloc) {}
    flat_set(sorted_unique_t, initializer_list<value_type> values, const Compare &comp,
             const allocator_type &alloc)
        : flat_set(sorted_unique, values.begin(), values.end(), comp, alloc) {}
    flat_set(sorted_unique_t, initializer_list<value_type> values, const allocator_type &alloc)
        : flat_set(sorted_unique, values.begin(), values.end(), Compare(), alloc) {}
    flat_set(sorted_unique_t, initializer_list<value_type> values, const Compare &comp)
        : flat_set(sorted_unique, values.begin(), values.end(), comp) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(initializer_list<value_type> values, const Alloc &alloc)
        : flat_set(values.begin(), values.end(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(initializer_list<value_type> values, const Compare &comp, const Alloc &alloc)
        : flat_set(values.begin(), values.end(), comp, alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, initializer_list<value_type> values, const Alloc &alloc)
        : flat_set(sorted_unique, values.begin(), values.end(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(sorted_unique_t, initializer_list<value_type> values, const Compare &comp,
             const Alloc &alloc)
        : flat_set(sorted_unique, values.begin(), values.end(), comp, alloc) {}
#if __cplusplus > 202002L
    template <typename R>
    flat_set(from_range_t, R &&range, const Compare &comp = Compare()) : comp_(comp), keys_() {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_set(from_range_t, R &&range, const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(alloc) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_set(from_range_t, R &&range, const allocator_type &alloc)
        : flat_set(from_range, std::forward<R>(range), Compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(from_range_t, R &&range, const Alloc &alloc)
        : flat_set(from_range, std::forward<R>(range), Compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<KeyContainer, Alloc>::value, int> = 0>
    flat_set(from_range_t, R &&range, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
#endif

    allocator_type get_allocator() const { return keys_.get_allocator(); }
    const KeyContainer &key_container() const noexcept { return keys_; }
    KeyContainer extract() && { return std::move(keys_); }
    void replace(KeyContainer &&values) {
        flat_set candidate(std::move(values), comp_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            keys_.swap(candidate.keys_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            candidate.clear();
            throw;
        }
#endif
    }
    flat_set &operator=(initializer_list<value_type> values) {
        clear();
        insert(values);
        return *this;
    }
    iterator begin() const noexcept { return keys_.begin(); }
    iterator cbegin() const noexcept { return keys_.cbegin(); }
    iterator end() const noexcept { return keys_.end(); }
    iterator cend() const noexcept { return keys_.cend(); }
    reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    reverse_iterator crbegin() const noexcept { return reverse_iterator(cend()); }
    reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
    reverse_iterator crend() const noexcept { return reverse_iterator(cbegin()); }
    bool empty() const noexcept { return keys_.empty(); }
    size_type size() const noexcept { return keys_.size(); }
    size_type max_size() const noexcept { return keys_.max_size(); }
    void clear() noexcept { keys_.clear(); }
    void swap(flat_set &other) noexcept(noexcept(keys_.swap(other.keys_)) &&
                                        noexcept(std::swap(comp_, other.comp_))) {
        if constexpr (noexcept(keys_.swap(other.keys_)) &&
                      noexcept(std::swap(comp_, other.comp_))) {
            keys_.swap(other.keys_);
            std::swap(comp_, other.comp_);
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                keys_.swap(other.keys_);
                std::swap(comp_, other.comp_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                clear();
                other.clear();
                throw;
            }
#endif
        }
    }
    key_compare key_comp() const { return comp_; }
    value_compare value_comp() const { return comp_; }

    pair<iterator, bool> insert(const value_type &value) { return insert_value(value); }
    pair<iterator, bool> insert(value_type &&value) { return insert_value(std::move(value)); }
    iterator insert(const_iterator, const value_type &value) { return insert(value).first; }
    iterator insert(const_iterator, value_type &&value) { return insert(std::move(value)).first; }
    template <typename InputIt> void insert(InputIt first, InputIt last) {
        for (; first != last; ++first)
            insert(*first);
    }
    void insert(initializer_list<value_type> values) { insert(values.begin(), values.end()); }
    void insert(sorted_unique_t, initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }
    template <typename InputIt> void insert(sorted_unique_t, InputIt first, InputIt last) {
        insert(first, last);
    }
#if __cplusplus > 202002L
    template <typename R> void insert_range(R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
    template <typename R> void insert_range(sorted_unique_t, R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
#endif
    template <typename... Args> pair<iterator, bool> emplace(Args &&...args) {
        return insert(value_type(std::forward<Args>(args)...));
    }
    template <typename... Args> iterator emplace_hint(const_iterator hint, Args &&...args) {
        (void)hint;
        return emplace(std::forward<Args>(args)...).first;
    }
    iterator erase(const_iterator position) { return keys_.erase(position); }
    iterator erase(const_iterator first, const_iterator last) { return keys_.erase(first, last); }
    size_type erase(const key_type &key) {
        auto it = find(key);
        if (it == end())
            return 0;
        erase(it);
        return 1;
    }
    template <typename K> size_type erase(const K &key) {
        auto it = find(key);
        if (it == end())
            return 0;
        erase(it);
        return 1;
    }
    template <typename K> iterator find(const K &key) const {
        const size_type i = lower(key);
        return i != size() && !comp_(key, keys_[i]) ? flat_detail::iter_at(keys_, i) : end();
    }
    template <typename K> size_type count(const K &key) const { return find(key) == end() ? 0 : 1; }
    template <typename K> bool contains(const K &key) const { return find(key) != end(); }
    template <typename K> iterator lower_bound(const K &key) const {
        return flat_detail::iter_at(keys_, lower(key));
    }
    template <typename K> iterator upper_bound(const K &key) const {
        return flat_detail::iter_at(keys_, upper(key));
    }
    template <typename K> pair<iterator, iterator> equal_range(const K &key) const {
        return pair<iterator, iterator>(lower_bound(key), upper_bound(key));
    }
    friend bool operator==(const flat_set &left, const flat_set &right) {
        if (left.size() != right.size())
            return false;
        auto a = left.begin();
        auto b = right.begin();
        for (; a != left.end(); ++a, ++b)
            if (!(*a == *b))
                return false;
        return true;
    }
    friend auto operator<=>(const flat_set &left, const flat_set &right)
        -> detail::synth_three_way_result_t<value_type> {
        return flat_detail::compare_sequences<value_type>(left.begin(), left.end(), right.begin(),
                                                          right.end());
    }
    friend void swap(flat_set &left, flat_set &right) noexcept(noexcept(left.swap(right))) {
        left.swap(right);
    }
};

template <typename Key, typename Compare = less<Key>, typename KeyContainer = vector<Key>>
class flat_multiset : public flat_set<Key, Compare, KeyContainer> {
    using base = flat_set<Key, Compare, KeyContainer>;

  public:
    using typename base::allocator_type;
    using typename base::const_iterator;
    using typename base::key_compare;
    using typename base::key_container_type;
    using typename base::key_type;
    using typename base::size_type;
    using typename base::value_type;
    flat_multiset() = default;
    explicit flat_multiset(const key_compare &comp) : base(comp) {}
    explicit flat_multiset(const allocator_type &alloc) : base(alloc) {}
    flat_multiset(const key_compare &comp, const allocator_type &alloc) : base(comp, alloc) {}
    flat_multiset(const flat_multiset &other, const allocator_type &alloc)
        : base(static_cast<const base &>(other), alloc) {}
    flat_multiset(flat_multiset &&other, const allocator_type &alloc)
        : base(static_cast<base &&>(other), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    explicit flat_multiset(const Alloc &alloc) : base(alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(const key_compare &comp, const Alloc &alloc) : base(comp, alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(const flat_multiset &other, const Alloc &alloc)
        : base(static_cast<const base &>(other), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(flat_multiset &&other, const Alloc &alloc)
        : base(static_cast<base &&>(other), alloc) {}
    explicit flat_multiset(key_container_type values, const key_compare &comp = key_compare())
        : base(comp) {
        sort(values.begin(), values.end(), comp);
        this->mutable_key_container() = std::move(values);
    }
    flat_multiset(const key_container_type &values, const allocator_type &alloc) : base(alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), values.begin(),
                                             values.end());
        sort(this->mutable_key_container().begin(), this->mutable_key_container().end(),
             this->key_comp());
    }
    flat_multiset(const key_container_type &values, const key_compare &comp,
                  const allocator_type &alloc)
        : base(comp, alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), values.begin(),
                                             values.end());
        sort(this->mutable_key_container().begin(), this->mutable_key_container().end(), comp);
    }
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(const key_container_type &values, const Alloc &alloc) : base(alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), values.begin(),
                                             values.end());
        sort(this->mutable_key_container().begin(), this->mutable_key_container().end(),
             this->key_comp());
    }
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(const key_container_type &values, const key_compare &comp, const Alloc &alloc)
        : base(comp, alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), values.begin(),
                                             values.end());
        sort(this->mutable_key_container().begin(), this->mutable_key_container().end(), comp);
    }
    flat_multiset(sorted_equivalent_t, const key_container_type &values,
                  const key_compare &comp = key_compare())
        : base(sorted_unique, values, comp) {}
    flat_multiset(sorted_equivalent_t, key_container_type &&values,
                  const key_compare &comp = key_compare())
        : base(sorted_unique, std::move(values), comp) {}
    flat_multiset(sorted_equivalent_t, const key_container_type &values, const key_compare &comp,
                  const allocator_type &alloc)
        : base(sorted_unique, values, comp, alloc) {}
    flat_multiset(sorted_equivalent_t, const key_container_type &values,
                  const allocator_type &alloc)
        : base(sorted_unique, values, key_compare(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, const key_container_type &values, const Alloc &alloc)
        : flat_multiset(sorted_equivalent, values, key_compare(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, const key_container_type &values, const key_compare &comp,
                  const Alloc &alloc)
        : base(sorted_unique, values, comp, alloc) {}
    template <typename InputIt>
    flat_multiset(sorted_equivalent_t, InputIt first, InputIt last,
                  const key_compare &comp = key_compare())
        : base(sorted_unique, first, last, comp) {}
    template <typename InputIt>
    flat_multiset(InputIt first, InputIt last, const key_compare &comp = key_compare())
        : base(comp) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multiset(InputIt first, InputIt last, const key_compare &comp, const allocator_type &alloc)
        : base(comp, alloc) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multiset(InputIt first, InputIt last, const allocator_type &alloc)
        : flat_multiset(first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(InputIt first, InputIt last, const Alloc &alloc)
        : flat_multiset(first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(InputIt first, InputIt last, const key_compare &comp, const Alloc &alloc)
        : base(comp, alloc) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multiset(sorted_equivalent_t, InputIt first, InputIt last, const key_compare &comp,
                  const allocator_type &alloc)
        : base(comp, alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), first, last);
    }
    template <typename InputIt>
    flat_multiset(sorted_equivalent_t, InputIt first, InputIt last, const allocator_type &alloc)
        : flat_multiset(sorted_equivalent, first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, InputIt first, InputIt last, const Alloc &alloc)
        : flat_multiset(sorted_equivalent, first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, InputIt first, InputIt last, const key_compare &comp,
                  const Alloc &alloc)
        : base(comp, alloc) {
        this->mutable_key_container().insert(this->mutable_key_container().end(), first, last);
    }
    flat_multiset(initializer_list<value_type> values, const key_compare &comp = key_compare())
        : flat_multiset(values.begin(), values.end(), comp) {}
    flat_multiset(initializer_list<value_type> values, const key_compare &comp,
                  const allocator_type &alloc)
        : flat_multiset(values.begin(), values.end(), comp, alloc) {}
    flat_multiset(initializer_list<value_type> values, const allocator_type &alloc)
        : flat_multiset(values.begin(), values.end(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(initializer_list<value_type> values, const Alloc &alloc)
        : flat_multiset(values.begin(), values.end(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(initializer_list<value_type> values, const key_compare &comp, const Alloc &alloc)
        : flat_multiset(values.begin(), values.end(), comp, alloc) {}
    flat_multiset(sorted_equivalent_t, initializer_list<value_type> values,
                  const key_compare &comp = key_compare())
        : flat_multiset(sorted_equivalent, values.begin(), values.end(), comp) {}
    flat_multiset(sorted_equivalent_t, initializer_list<value_type> values, const key_compare &comp,
                  const allocator_type &alloc)
        : flat_multiset(sorted_equivalent, values.begin(), values.end(), comp, alloc) {}
    flat_multiset(sorted_equivalent_t, initializer_list<value_type> values,
                  const allocator_type &alloc)
        : flat_multiset(sorted_equivalent, values.begin(), values.end(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, initializer_list<value_type> values, const Alloc &alloc)
        : flat_multiset(sorted_equivalent, values.begin(), values.end(), alloc) {}
    template <typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(sorted_equivalent_t, initializer_list<value_type> values, const key_compare &comp,
                  const Alloc &alloc)
        : flat_multiset(sorted_equivalent, values.begin(), values.end(), comp, alloc) {}
#if __cplusplus > 202002L
    template <typename R>
    flat_multiset(from_range_t, R &&range, const key_compare &comp = key_compare()) : base(comp) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_multiset(from_range_t, R &&range, const key_compare &comp, const allocator_type &alloc)
        : base(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_multiset(from_range_t, R &&range, const allocator_type &alloc)
        : flat_multiset(from_range, std::forward<R>(range), key_compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(from_range_t, R &&range, const Alloc &alloc)
        : flat_multiset(from_range, std::forward<R>(range), key_compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value, int> = 0>
    flat_multiset(from_range_t, R &&range, const key_compare &comp, const Alloc &alloc)
        : base(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif
    using base::begin;
    using base::contains;
    using base::end;
    using base::equal_range;
    using base::find;
    using base::key_comp;
    using base::lower_bound;
    using base::upper_bound;
    const_iterator insert(const value_type &value) { return this->insert_equivalent(value); }
    const_iterator insert(value_type &&value) { return this->insert_equivalent(std::move(value)); }
    const_iterator insert(const_iterator, const value_type &value) { return insert(value); }
    const_iterator insert(const_iterator, value_type &&value) { return insert(std::move(value)); }
    template <typename InputIt> void insert(InputIt first, InputIt last) {
        this->insert_equivalent_range(first, last);
    }
    void insert(initializer_list<value_type> values) { insert(values.begin(), values.end()); }
    void insert(sorted_equivalent_t, initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }
    template <typename InputIt> void insert(sorted_equivalent_t, InputIt first, InputIt last) {
        insert(first, last);
    }
#if __cplusplus > 202002L
    template <typename R> void insert_range(R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
    template <typename R> void insert_range(sorted_equivalent_t, R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
#endif
    template <typename... Args> const_iterator emplace(Args &&...args) {
        return insert(value_type(std::forward<Args>(args)...));
    }
    template <typename... Args> const_iterator emplace_hint(const_iterator, Args &&...args) {
        return emplace(std::forward<Args>(args)...);
    }
    size_type count(const key_type &key) const {
        auto range = this->equal_range(key);
        return static_cast<size_type>(range.second - range.first);
    }
    size_type erase(const key_type &key) {
        auto range = this->equal_range(key);
        const auto count = static_cast<size_type>(range.second - range.first);
        this->mutable_key_container().erase(range.first, range.second);
        return count;
    }
    const_iterator erase(const_iterator position) {
        return this->mutable_key_container().erase(position);
    }
    const_iterator erase(const_iterator first, const_iterator last) {
        return this->mutable_key_container().erase(first, last);
    }
    void replace(key_container_type &&values) {
        flat_multiset candidate(std::move(values), this->key_comp());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            this->swap(candidate);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            this->clear();
            candidate.clear();
            throw;
        }
#endif
    }
    flat_multiset &operator=(initializer_list<value_type> values) {
        this->clear();
        insert(values);
        return *this;
    }
};

template <typename Key, typename T, typename Compare = less<Key>,
          typename KeyContainer = vector<Key>, typename MappedContainer = vector<T>>
class flat_map {
  public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = pair<Key, T>;
    using key_compare = Compare;
    using key_container_type = KeyContainer;
    using mapped_container_type = MappedContainer;
    using allocator_type = typename allocator_traits<
        typename KeyContainer::allocator_type>::template rebind_alloc<value_type>;
    using key_allocator_type =
        typename allocator_traits<allocator_type>::template rebind_alloc<key_type>;
    using mapped_allocator_type =
        typename allocator_traits<allocator_type>::template rebind_alloc<mapped_type>;
    using size_type = typename KeyContainer::size_type;
    using difference_type = typename KeyContainer::difference_type;
    using reference = pair<const Key &, T &>;
    using const_reference = pair<const Key &, const T &>;
    static_assert(is_same<typename KeyContainer::value_type, Key>::value,
                  "flat_map Key must match KeyContainer::value_type");
    static_assert(is_same<typename MappedContainer::value_type, T>::value,
                  "flat_map T must match MappedContainer::value_type");

  private:
    Compare comp_;
    KeyContainer keys_;
    MappedContainer mapped_;

    template <bool Const> class basic_iterator {
        template <bool> friend class basic_iterator;
        friend class flat_map;
        using owner_type = conditional_t<Const, const flat_map, flat_map>;
        owner_type *owner_;
        size_type index_;
        basic_iterator(owner_type *owner, size_type index) : owner_(owner), index_(index) {}

      public:
        using iterator_category = random_access_iterator_tag;
        using iterator_concept = random_access_iterator_tag;
        using value_type = typename flat_map::value_type;
        using difference_type = typename flat_map::difference_type;
        using reference =
            conditional_t<Const, typename flat_map::const_reference, typename flat_map::reference>;
        using pointer = void;
        basic_iterator() : owner_(nullptr), index_(0) {}
        template <bool C = Const, enable_if_t<C, int> = 0>
        basic_iterator(const basic_iterator<false> &other)
            : owner_(other.owner_), index_(other.index_) {}
        reference operator*() const {
            return reference(owner_->keys_[index_], owner_->mapped_[index_]);
        }
        struct arrow_proxy {
            reference value;
            explicit arrow_proxy(reference v) : value(v) {}
            reference *operator->() { return &value; }
        };
        arrow_proxy operator->() const { return arrow_proxy(operator*()); }
        reference operator[](difference_type n) const { return *(*this + n); }
        basic_iterator &operator++() {
            ++index_;
            return *this;
        }
        basic_iterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        basic_iterator &operator--() {
            --index_;
            return *this;
        }
        basic_iterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }
        basic_iterator &operator+=(difference_type n) {
            index_ = static_cast<size_type>(index_ + n);
            return *this;
        }
        basic_iterator &operator-=(difference_type n) {
            index_ = static_cast<size_type>(index_ - n);
            return *this;
        }
        friend basic_iterator operator+(basic_iterator i, difference_type n) {
            i += n;
            return i;
        }
        friend basic_iterator operator+(difference_type n, basic_iterator i) {
            i += n;
            return i;
        }
        friend basic_iterator operator-(basic_iterator i, difference_type n) {
            i -= n;
            return i;
        }
        template <bool C> difference_type operator-(const basic_iterator<C> &other) const {
            return static_cast<difference_type>(index_) -
                   static_cast<difference_type>(other.index_);
        }
        template <bool C> bool operator==(const basic_iterator<C> &other) const {
            return owner_ == other.owner_ && index_ == other.index_;
        }
        template <bool C> bool operator!=(const basic_iterator<C> &other) const {
            return !(*this == other);
        }
        template <bool C> bool operator<(const basic_iterator<C> &other) const {
            return index_ < other.index_;
        }
        template <bool C> bool operator>(const basic_iterator<C> &other) const {
            return other < *this;
        }
        template <bool C> bool operator<=(const basic_iterator<C> &other) const {
            return !(other < *this);
        }
        template <bool C> bool operator>=(const basic_iterator<C> &other) const {
            return !(*this < other);
        }
    };

  public:
    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    struct containers {
        key_container_type keys;
        mapped_container_type values;
    };

  private:
    template <typename K> size_type lower_index(const K &key) const {
        return static_cast<size_type>(flat_detail::lower_index(keys_, key, comp_));
    }
    template <typename K> size_type upper_index(const K &key) const {
        return static_cast<size_type>(flat_detail::upper_index(keys_, key, comp_));
    }
    template <typename K, typename V>
    iterator insert_pair(K &&key, V &&mapped, bool allow_equivalent) {
        const size_type index = allow_equivalent ? upper_index(key) : lower_index(key);
        if (!allow_equivalent && index != size() && !comp_(key, keys_[index]))
            return iterator(this, index);
        keys_.insert(flat_detail::iter_at(keys_, index), std::forward<K>(key));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            mapped_.insert(flat_detail::iter_at(mapped_, index), std::forward<V>(mapped));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            if (mapped_.size() == keys_.size()) {
                mapped_.erase(flat_detail::iter_at(mapped_, index));
                keys_.erase(flat_detail::iter_at(keys_, index));
            } else if (mapped_.size() + 1 == keys_.size()) {
                keys_.erase(flat_detail::iter_at(keys_, index));
            } else {
                clear();
            }
            throw;
        }
#endif
        return iterator(this, index);
    }
    template <typename InputIt> void insert_range_impl(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            auto &&value = *first;
            insert_pair(std::forward<decltype(value)>(value).first,
                        std::forward<decltype(value)>(value).second, false);
        }
    }
    template <typename InputIt> void insert_sorted_range(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            auto &&value = *first;
            keys_.insert(keys_.end(), std::forward<decltype(value)>(value).first);
            mapped_.insert(mapped_.end(), std::forward<decltype(value)>(value).second);
        }
    }
    void normalize_pairs(bool keep_equivalent) {
        vector<value_type> ordered;
        ordered.reserve(keys_.size());
        for (size_type i = 0; i < keys_.size(); ++i)
            ordered.emplace_back(std::move(keys_[i]), std::move(mapped_[i]));
        stable_sort(
            ordered.begin(), ordered.end(),
            [this](const value_type &a, const value_type &b) { return comp_(a.first, b.first); });
        KeyContainer new_keys(keys_.get_allocator());
        MappedContainer new_mapped(mapped_.get_allocator());
        for (size_type i = 0; i < ordered.size(); ++i) {
            if (!keep_equivalent && i != 0 && !comp_(ordered[i - 1].first, ordered[i].first) &&
                !comp_(ordered[i].first, ordered[i - 1].first))
                continue;
            new_keys.insert(new_keys.end(), std::move(ordered[i].first));
            new_mapped.insert(new_mapped.end(), std::move(ordered[i].second));
        }
        keys_.swap(new_keys);
        mapped_.swap(new_mapped);
    }
    template <typename Alloc>
    using allocator_enabled = enable_if_t<uses_allocator<KeyContainer, Alloc>::value &&
                                              uses_allocator<MappedContainer, Alloc>::value,
                                          int>;

  protected:
    template <typename K, typename V> iterator insert_equivalent(K &&key, V &&mapped) {
        return insert_pair(std::forward<K>(key), std::forward<V>(mapped), true);
    }
    KeyContainer &mutable_keys() noexcept { return keys_; }
    MappedContainer &mutable_mapped() noexcept { return mapped_; }

  public:
    flat_map() : comp_(), keys_(), mapped_() {}
    explicit flat_map(const Compare &comp) : comp_(comp), keys_(), mapped_() {}
    flat_map(const flat_map &) = default;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    flat_map(flat_map &&other) try
        : comp_(std::move(other.comp_)), keys_(std::move(other.keys_)),
          mapped_(std::move(other.mapped_)) {
        other.clear();
    } catch (...) {
        other.clear();
        throw;
    }
#else
    flat_map(flat_map &&other)
        : comp_(std::move(other.comp_)), keys_(std::move(other.keys_)),
          mapped_(std::move(other.mapped_)) {
        other.clear();
    }
#endif
    flat_map &operator=(const flat_map &other) {
        if (this != &other) {
            flat_map candidate(other);
            swap(candidate);
        }
        return *this;
    }
    flat_map &operator=(flat_map &&other) {
        if (this != &other) {
            flat_map candidate(std::move(other));
            swap(candidate);
        }
        return *this;
    }
    flat_map(KeyContainer keys, MappedContainer mapped, const Compare &comp = Compare())
        : comp_(comp), keys_(std::move(keys)), mapped_(std::move(mapped)) {
        normalize_pairs(false);
    }
    explicit flat_map(const allocator_type &alloc)
        : comp_(), keys_(key_allocator_type(alloc)), mapped_(mapped_allocator_type(alloc)) {}
    flat_map(const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(key_allocator_type(alloc)), mapped_(mapped_allocator_type(alloc)) {}
    flat_map(const flat_map &other, const allocator_type &alloc)
        : comp_(other.comp_),
          keys_(flat_detail::copy_container<KeyContainer>(other.keys_.begin(), other.keys_.end(),
                                                          key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              other.mapped_.begin(), other.mapped_.end(), mapped_allocator_type(alloc))) {}
    flat_map(flat_map &&other, const allocator_type &alloc)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try
#endif
        : comp_(std::move(other.comp_)),
          keys_(flat_detail::copy_container<KeyContainer>(
              std::make_move_iterator(other.keys_.begin()),
              std::make_move_iterator(other.keys_.end()), key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              std::make_move_iterator(other.mapped_.begin()),
              std::make_move_iterator(other.mapped_.end()), mapped_allocator_type(alloc))) {
        other.clear();
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    catch (...) {
        other.clear();
        throw;
    }
#endif
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    explicit flat_map(const Alloc &alloc)
        : comp_(), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)),
          mapped_(flat_detail::sequence_allocator<MappedContainer>(alloc)) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)),
          mapped_(flat_detail::sequence_allocator<MappedContainer>(alloc)) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(const flat_map &other, const Alloc &alloc)
        : comp_(other.comp_), keys_(flat_detail::copy_container<KeyContainer>(
                                  other.keys_.begin(), other.keys_.end(),
                                  flat_detail::sequence_allocator<KeyContainer>(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              other.mapped_.begin(), other.mapped_.end(),
              flat_detail::sequence_allocator<MappedContainer>(alloc))) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(flat_map &&other, const Alloc &alloc)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try
#endif
        : comp_(std::move(other.comp_)), keys_(flat_detail::copy_container<KeyContainer>(
                                             std::make_move_iterator(other.keys_.begin()),
                                             std::make_move_iterator(other.keys_.end()),
                                             flat_detail::sequence_allocator<KeyContainer>(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              std::make_move_iterator(other.mapped_.begin()),
              std::make_move_iterator(other.mapped_.end()),
              flat_detail::sequence_allocator<MappedContainer>(alloc))) {
        other.clear();
    }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    catch (...) {
        other.clear();
        throw;
    }
#endif
    flat_map(const Compare &comp, const KeyContainer &keys, const MappedContainer &mapped)
        : flat_map(KeyContainer(keys), MappedContainer(mapped), comp) {}
    flat_map(const Compare &comp, KeyContainer &&keys, MappedContainer &&mapped)
        : flat_map(std::move(keys), std::move(mapped), comp) {}
    flat_map(sorted_equivalent_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp = Compare())
        : comp_(comp), keys_(keys), mapped_(mapped) {
        normalize_pairs(true);
    }
    flat_map(sorted_equivalent_t, KeyContainer &&keys, MappedContainer &&mapped,
             const Compare &comp = Compare())
        : comp_(comp), keys_(std::move(keys)), mapped_(std::move(mapped)) {
        normalize_pairs(true);
    }
    flat_map(sorted_equivalent_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(flat_detail::copy_container<KeyContainer>(keys.begin(), keys.end(),
                                                                       key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(mapped.begin(), mapped.end(),
                                                               mapped_allocator_type(alloc))) {
        normalize_pairs(true);
    }
    flat_map(sorted_equivalent_t, const KeyContainer &keys, const MappedContainer &mapped,
             const allocator_type &alloc)
        : flat_map(sorted_equivalent, keys, mapped, Compare(), alloc) {}
    flat_map(sorted_unique_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp = Compare())
        : comp_(comp), keys_(keys), mapped_(mapped) {}
    flat_map(sorted_unique_t, KeyContainer &&keys, MappedContainer &&mapped,
             const Compare &comp = Compare())
        : comp_(comp), keys_(std::move(keys)), mapped_(std::move(mapped)) {}
    flat_map(const KeyContainer &keys, const MappedContainer &mapped, const allocator_type &alloc)
        : flat_map(Compare(), keys, mapped, alloc) {}
    flat_map(const KeyContainer &keys, const MappedContainer &mapped, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp), keys_(flat_detail::copy_container<KeyContainer>(keys.begin(), keys.end(),
                                                                       key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(mapped.begin(), mapped.end(),
                                                               mapped_allocator_type(alloc))) {
        normalize_pairs(false);
    }
    flat_map(sorted_unique_t, const KeyContainer &keys, const MappedContainer &mapped,
             const allocator_type &alloc)
        : flat_map(sorted_unique, keys, mapped, Compare(), alloc) {}
    flat_map(sorted_unique_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(flat_detail::copy_container<KeyContainer>(keys.begin(), keys.end(),
                                                                       key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(mapped.begin(), mapped.end(),
                                                               mapped_allocator_type(alloc))) {}
    flat_map(sorted_unique_t, KeyContainer &&keys, MappedContainer &&mapped, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(std::make_move_iterator(keys.begin()),
                                                          std::make_move_iterator(keys.end()),
                                                          key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              std::make_move_iterator(mapped.begin()), std::make_move_iterator(mapped.end()),
              mapped_allocator_type(alloc))) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(const KeyContainer &keys, const MappedContainer &mapped, const Alloc &alloc)
        : flat_map(keys, mapped, Compare(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(const KeyContainer &keys, const MappedContainer &mapped, const Compare &comp,
             const Alloc &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(
              keys.begin(), keys.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              mapped.begin(), mapped.end(),
              flat_detail::sequence_allocator<MappedContainer>(alloc))) {
        normalize_pairs(false);
    }
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Alloc &alloc)
        : flat_map(sorted_unique, keys, mapped, Compare(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp, const Alloc &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(
              keys.begin(), keys.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              mapped.begin(), mapped.end(),
              flat_detail::sequence_allocator<MappedContainer>(alloc))) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_equivalent_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Alloc &alloc)
        : flat_map(sorted_equivalent, keys, mapped, Compare(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_equivalent_t, const KeyContainer &keys, const MappedContainer &mapped,
             const Compare &comp, const Alloc &alloc)
        : comp_(comp),
          keys_(flat_detail::copy_container<KeyContainer>(
              keys.begin(), keys.end(), flat_detail::sequence_allocator<KeyContainer>(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(
              mapped.begin(), mapped.end(),
              flat_detail::sequence_allocator<MappedContainer>(alloc))) {
        normalize_pairs(true);
    }
    flat_map(const Compare &comp, const KeyContainer &keys, const MappedContainer &mapped,
             const allocator_type &alloc)
        : comp_(comp), keys_(flat_detail::copy_container<KeyContainer>(keys.begin(), keys.end(),
                                                                       key_allocator_type(alloc))),
          mapped_(flat_detail::copy_container<MappedContainer>(mapped.begin(), mapped.end(),
                                                               mapped_allocator_type(alloc))) {
        normalize_pairs(false);
    }
    template <typename InputIt>
    flat_map(InputIt first, InputIt last, const Compare &comp = Compare())
        : comp_(comp), keys_(), mapped_() {
        insert_range_impl(first, last);
    }
    template <typename InputIt>
    flat_map(InputIt first, InputIt last, const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(key_allocator_type(alloc)), mapped_(mapped_allocator_type(alloc)) {
        insert_range_impl(first, last);
    }
    template <typename InputIt>
    flat_map(InputIt first, InputIt last, const allocator_type &alloc)
        : flat_map(first, last, Compare(), alloc) {}
    template <typename InputIt>
    flat_map(sorted_unique_t, InputIt first, InputIt last, const Compare &comp = Compare())
        : comp_(comp), keys_(), mapped_() {
        insert_sorted_range(first, last);
    }
    template <typename InputIt>
    flat_map(sorted_unique_t, InputIt first, InputIt last, const Compare &comp,
             const allocator_type &alloc)
        : comp_(comp), keys_(key_allocator_type(alloc)), mapped_(mapped_allocator_type(alloc)) {
        insert_sorted_range(first, last);
    }
    template <typename InputIt>
    flat_map(sorted_unique_t, InputIt first, InputIt last, const allocator_type &alloc)
        : flat_map(sorted_unique, first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(InputIt first, InputIt last, const Alloc &alloc)
        : flat_map(first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(InputIt first, InputIt last, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)),
          mapped_(flat_detail::sequence_allocator<MappedContainer>(alloc)) {
        insert_range_impl(first, last);
    }
    template <typename InputIt, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, InputIt first, InputIt last, const Alloc &alloc)
        : flat_map(sorted_unique, first, last, Compare(), alloc) {}
    template <typename InputIt, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, InputIt first, InputIt last, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)),
          mapped_(flat_detail::sequence_allocator<MappedContainer>(alloc)) {
        insert_sorted_range(first, last);
    }
    flat_map(initializer_list<value_type> values, const Compare &comp = Compare())
        : flat_map(values.begin(), values.end(), comp) {}
    flat_map(initializer_list<value_type> values, const Compare &comp, const allocator_type &alloc)
        : flat_map(values.begin(), values.end(), comp, alloc) {}
    flat_map(initializer_list<value_type> values, const allocator_type &alloc)
        : flat_map(values.begin(), values.end(), alloc) {}
    flat_map(sorted_unique_t, initializer_list<value_type> values, const Compare &comp = Compare())
        : flat_map(sorted_unique, values.begin(), values.end(), comp) {}
    flat_map(sorted_unique_t, initializer_list<value_type> values, const Compare &comp,
             const allocator_type &alloc)
        : flat_map(sorted_unique, values.begin(), values.end(), comp, alloc) {}
    flat_map(sorted_unique_t, initializer_list<value_type> values, const allocator_type &alloc)
        : flat_map(sorted_unique, values.begin(), values.end(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(initializer_list<value_type> values, const Alloc &alloc)
        : flat_map(values.begin(), values.end(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(initializer_list<value_type> values, const Compare &comp, const Alloc &alloc)
        : flat_map(values.begin(), values.end(), comp, alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, initializer_list<value_type> values, const Alloc &alloc)
        : flat_map(sorted_unique, values.begin(), values.end(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_unique_t, initializer_list<value_type> values, const Compare &comp,
             const Alloc &alloc)
        : flat_map(sorted_unique, values.begin(), values.end(), comp, alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_equivalent_t, initializer_list<value_type> values, const Alloc &alloc)
        : flat_map(sorted_equivalent, values.begin(), values.end(), Compare(), alloc) {}
    template <typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(sorted_equivalent_t, initializer_list<value_type> values, const Compare &comp,
             const Alloc &alloc)
        : flat_map(sorted_equivalent, values.begin(), values.end(), comp, alloc) {}
#if __cplusplus > 202002L
    template <typename R>
    flat_map(from_range_t, R &&range, const Compare &comp = Compare())
        : comp_(comp), keys_(), mapped_() {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_map(from_range_t, R &&range, const Compare &comp, const allocator_type &alloc)
        : comp_(comp), keys_(key_allocator_type(alloc)), mapped_(mapped_allocator_type(alloc)) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_map(from_range_t, R &&range, const allocator_type &alloc)
        : flat_map(from_range, std::forward<R>(range), Compare(), alloc) {}
    template <typename R, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(from_range_t, R &&range, const Alloc &alloc)
        : flat_map(from_range, std::forward<R>(range), Compare(), alloc) {}
    template <typename R, typename Alloc, allocator_enabled<Alloc> = 0>
    flat_map(from_range_t, R &&range, const Compare &comp, const Alloc &alloc)
        : comp_(comp), keys_(flat_detail::sequence_allocator<KeyContainer>(alloc)),
          mapped_(flat_detail::sequence_allocator<MappedContainer>(alloc)) {
        insert_range(std::forward<R>(range));
    }
#endif

    allocator_type get_allocator() const { return allocator_type(keys_.get_allocator()); }
    const KeyContainer &keys() const noexcept { return keys_; }
    const MappedContainer &values() const noexcept { return mapped_; }
    const KeyContainer &key_container() const noexcept { return keys_; }
    const MappedContainer &mapped_container() const noexcept { return mapped_; }
    containers extract() && { return containers{std::move(keys_), std::move(mapped_)}; }
    KeyContainer extract_keys() && { return std::move(keys_); }
    MappedContainer extract_mapped() && { return std::move(mapped_); }
    void replace(KeyContainer keys, MappedContainer mapped) {
        flat_map candidate(sorted_unique, std::move(keys), std::move(mapped), comp_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            keys_.swap(candidate.keys_);
            mapped_.swap(candidate.mapped_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            candidate.clear();
            throw;
        }
#endif
    }
    flat_map &operator=(initializer_list<value_type> values) {
        clear();
        insert(values);
        return *this;
    }
    iterator begin() noexcept { return iterator(this, 0); }
    const_iterator begin() const noexcept { return const_iterator(this, 0); }
    const_iterator cbegin() const noexcept { return begin(); }
    iterator end() noexcept { return iterator(this, size()); }
    const_iterator end() const noexcept { return const_iterator(this, size()); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    bool empty() const noexcept { return keys_.empty(); }
    size_type size() const noexcept { return keys_.size(); }
    size_type max_size() const noexcept {
        return keys_.max_size() < mapped_.max_size() ? keys_.max_size() : mapped_.max_size();
    }
    void clear() noexcept {
        keys_.clear();
        mapped_.clear();
    }
    void swap(flat_map &other) noexcept(noexcept(keys_.swap(other.keys_)) &&
                                        noexcept(mapped_.swap(other.mapped_)) &&
                                        noexcept(std::swap(comp_, other.comp_))) {
        if constexpr (noexcept(keys_.swap(other.keys_)) && noexcept(mapped_.swap(other.mapped_)) &&
                      noexcept(std::swap(comp_, other.comp_))) {
            keys_.swap(other.keys_);
            mapped_.swap(other.mapped_);
            std::swap(comp_, other.comp_);
        } else {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
#endif
                keys_.swap(other.keys_);
                mapped_.swap(other.mapped_);
                std::swap(comp_, other.comp_);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            } catch (...) {
                clear();
                other.clear();
                throw;
            }
#endif
        }
    }
    key_compare key_comp() const { return comp_; }
    class value_compare {
        Compare comp_;

      public:
        explicit value_compare(Compare comp) : comp_(std::move(comp)) {}
        bool operator()(const_reference a, const_reference b) const {
            return comp_(a.first, b.first);
        }
    };
    value_compare value_comp() const { return value_compare(comp_); }

    pair<iterator, bool> insert(const value_type &value) {
        auto index = lower_index(value.first);
        if (index != size() && !comp_(value.first, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(value.first, value.second, false), true};
    }
    pair<iterator, bool> insert(value_type &&value) {
        auto index = lower_index(value.first);
        if (index != size() && !comp_(value.first, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(std::move(value.first), std::move(value.second), false), true};
    }
    template <typename P> pair<iterator, bool> insert(P &&value) {
        auto index = lower_index(value.first);
        if (index != size() && !comp_(value.first, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(std::forward<P>(value).first, std::forward<P>(value).second, false),
                true};
    }
    iterator insert(const_iterator, const value_type &value) { return insert(value).first; }
    iterator insert(const_iterator, value_type &&value) { return insert(std::move(value)).first; }
    template <typename InputIt> void insert(InputIt first, InputIt last) {
        insert_range_impl(first, last);
    }
    void insert(initializer_list<value_type> values) { insert(values.begin(), values.end()); }
    void insert(sorted_unique_t, initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }
    template <typename InputIt> void insert(sorted_unique_t, InputIt first, InputIt last) {
        insert_sorted_range(first, last);
    }
#if __cplusplus > 202002L
    template <typename R> void insert_range(R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
    template <typename R> void insert_range(sorted_unique_t, R &&range) {
        for (auto &&value : range)
            insert(std::forward<decltype(value)>(value));
    }
#endif
    template <typename... Args> pair<iterator, bool> emplace(Args &&...args) {
        pair<key_type, mapped_type> value(std::forward<Args>(args)...);
        auto index = lower_index(value.first);
        if (index != size() && !comp_(value.first, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(std::move(value.first), std::move(value.second), false), true};
    }
    template <typename... Args> iterator emplace_hint(const_iterator, Args &&...args) {
        return emplace(std::forward<Args>(args)...).first;
    }
    template <typename... Args>
    pair<iterator, bool> try_emplace(const key_type &key, Args &&...args) {
        auto index = lower_index(key);
        if (index != size() && !comp_(key, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(key, mapped_type(std::forward<Args>(args)...), false), true};
    }
    template <typename... Args> pair<iterator, bool> try_emplace(key_type &&key, Args &&...args) {
        auto index = lower_index(key);
        if (index != size() && !comp_(key, keys_[index]))
            return {iterator(this, index), false};
        return {insert_pair(std::move(key), mapped_type(std::forward<Args>(args)...), false), true};
    }
    template <typename... Args>
    iterator try_emplace(const_iterator, const key_type &key, Args &&...args) {
        return try_emplace(key, std::forward<Args>(args)...).first;
    }
    template <typename... Args>
    iterator try_emplace(const_iterator, key_type &&key, Args &&...args) {
        return try_emplace(std::move(key), std::forward<Args>(args)...).first;
    }
    template <typename M> pair<iterator, bool> insert_or_assign(const key_type &key, M &&value) {
        auto index = lower_index(key);
        if (index != size() && !comp_(key, keys_[index])) {
            mapped_[index] = std::forward<M>(value);
            return {iterator(this, index), false};
        }
        return {insert_pair(key, std::forward<M>(value), false), true};
    }
    template <typename M> pair<iterator, bool> insert_or_assign(key_type &&key, M &&value) {
        auto index = lower_index(key);
        if (index != size() && !comp_(key, keys_[index])) {
            mapped_[index] = std::forward<M>(value);
            return {iterator(this, index), false};
        }
        return {insert_pair(std::move(key), std::forward<M>(value), false), true};
    }
    template <typename M>
    iterator insert_or_assign(const_iterator, const key_type &key, M &&value) {
        return insert_or_assign(key, std::forward<M>(value)).first;
    }
    template <typename M> iterator insert_or_assign(const_iterator, key_type &&key, M &&value) {
        return insert_or_assign(std::move(key), std::forward<M>(value)).first;
    }
    mapped_type &operator[](const key_type &key) { return try_emplace(key).first->second; }
    mapped_type &operator[](key_type &&key) { return try_emplace(std::move(key)).first->second; }
    mapped_type &at(const key_type &key) {
        auto it = find(key);
        if (it == end())
            flat_detail::at_failure();
        return it->second;
    }
    const mapped_type &at(const key_type &key) const {
        auto it = find(key);
        if (it == end())
            flat_detail::at_failure();
        return it->second;
    }
    iterator erase(const_iterator position) {
        if (position.owner_ != this || position.index_ >= size())
            return end();
        auto index = position.index_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            keys_.erase(flat_detail::iter_at(keys_, index));
            mapped_.erase(flat_detail::iter_at(mapped_, index));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
        return iterator(this, index);
    }
    iterator erase(const_iterator first, const_iterator last) {
        if (first.owner_ != this || last.owner_ != this || first.index_ > last.index_ ||
            last.index_ > size())
            return end();
        auto a = first.index_, b = last.index_;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            keys_.erase(flat_detail::iter_at(keys_, a), flat_detail::iter_at(keys_, b));
            mapped_.erase(flat_detail::iter_at(mapped_, a), flat_detail::iter_at(mapped_, b));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            clear();
            throw;
        }
#endif
        return iterator(this, a);
    }
    size_type erase(const key_type &key) {
        auto range = equal_range(key);
        auto n = static_cast<size_type>(range.second - range.first);
        erase(range.first, range.second);
        return n;
    }
    template <typename K> iterator find(const K &key) {
        auto i = lower_index(key);
        return i != size() && !comp_(key, keys_[i]) ? iterator(this, i) : end();
    }
    template <typename K> const_iterator find(const K &key) const {
        auto i = lower_index(key);
        return i != size() && !comp_(key, keys_[i]) ? const_iterator(this, i) : end();
    }
    template <typename K> size_type count(const K &key) const { return find(key) == end() ? 0 : 1; }
    template <typename K> bool contains(const K &key) const { return find(key) != end(); }
    template <typename K> iterator lower_bound(const K &key) {
        return iterator(this, lower_index(key));
    }
    template <typename K> const_iterator lower_bound(const K &key) const {
        return const_iterator(this, lower_index(key));
    }
    template <typename K> iterator upper_bound(const K &key) {
        return iterator(this, upper_index(key));
    }
    template <typename K> const_iterator upper_bound(const K &key) const {
        return const_iterator(this, upper_index(key));
    }
    template <typename K> pair<iterator, iterator> equal_range(const K &key) {
        return {lower_bound(key), upper_bound(key)};
    }
    template <typename K> pair<const_iterator, const_iterator> equal_range(const K &key) const {
        return {lower_bound(key), upper_bound(key)};
    }
    friend bool operator==(const flat_map &left, const flat_map &right) {
        if (left.size() != right.size())
            return false;
        auto a = left.begin();
        auto b = right.begin();
        for (; a != left.end(); ++a, ++b)
            if (!(*a == *b))
                return false;
        return true;
    }
    friend auto operator<=>(const flat_map &left, const flat_map &right)
        -> detail::synth_three_way_result_t<value_type> {
        return flat_detail::compare_sequences<value_type>(left.begin(), left.end(), right.begin(),
                                                          right.end());
    }
    friend void swap(flat_map &left, flat_map &right) noexcept(noexcept(left.swap(right))) {
        left.swap(right);
    }
};

template <typename Key, typename T, typename Compare = less<Key>,
          typename KeyContainer = vector<Key>, typename MappedContainer = vector<T>>
class flat_multimap : public flat_map<Key, T, Compare, KeyContainer, MappedContainer> {
    using base = flat_map<Key, T, Compare, KeyContainer, MappedContainer>;

  public:
    using typename base::allocator_type;
    using typename base::const_iterator;
    using typename base::iterator;
    using typename base::key_compare;
    using typename base::key_container_type;
    using typename base::key_type;
    using typename base::mapped_container_type;
    using typename base::mapped_type;
    using typename base::size_type;
    using typename base::value_type;
    flat_multimap() = default;
    explicit flat_multimap(const key_compare &comp) : base(comp) {}
    explicit flat_multimap(const allocator_type &alloc) : base(alloc) {}
    flat_multimap(const key_compare &comp, const allocator_type &alloc) : base(comp, alloc) {}
    flat_multimap(const flat_multimap &other, const allocator_type &alloc)
        : base(static_cast<const base &>(other), alloc) {}
    flat_multimap(flat_multimap &&other, const allocator_type &alloc)
        : base(static_cast<base &&>(other), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    explicit flat_multimap(const Alloc &alloc) : base(alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(const key_compare &comp, const Alloc &alloc) : base(comp, alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(const flat_multimap &other, const Alloc &alloc)
        : base(static_cast<const base &>(other), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(flat_multimap &&other, const Alloc &alloc)
        : base(static_cast<base &&>(other), alloc) {}
    template <typename InputIt>
    flat_multimap(InputIt first, InputIt last, const key_compare &comp = key_compare())
        : base(comp) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multimap(InputIt first, InputIt last, const key_compare &comp, const allocator_type &alloc)
        : base(comp, alloc) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multimap(InputIt first, InputIt last, const allocator_type &alloc)
        : flat_multimap(first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(InputIt first, InputIt last, const Alloc &alloc)
        : flat_multimap(first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(InputIt first, InputIt last, const key_compare &comp, const Alloc &alloc)
        : base(comp, alloc) {
        insert(first, last);
    }
    template <typename InputIt>
    flat_multimap(sorted_equivalent_t, InputIt first, InputIt last,
                  const key_compare &comp = key_compare())
        : base(sorted_unique, first, last, comp) {}
    template <typename InputIt>
    flat_multimap(sorted_equivalent_t, InputIt first, InputIt last, const key_compare &comp,
                  const allocator_type &alloc)
        : base(sorted_unique, first, last, comp, alloc) {}
    template <typename InputIt>
    flat_multimap(sorted_equivalent_t, InputIt first, InputIt last, const allocator_type &alloc)
        : flat_multimap(sorted_equivalent, first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(sorted_equivalent_t, InputIt first, InputIt last, const Alloc &alloc)
        : flat_multimap(sorted_equivalent, first, last, key_compare(), alloc) {}
    template <typename InputIt, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(sorted_equivalent_t, InputIt first, InputIt last, const key_compare &comp,
                  const Alloc &alloc)
        : base(sorted_unique, first, last, comp, alloc) {}
    flat_multimap(initializer_list<value_type> values, const key_compare &comp = key_compare())
        : flat_multimap(values.begin(), values.end(), comp) {}
    flat_multimap(initializer_list<value_type> values, const key_compare &comp,
                  const allocator_type &alloc)
        : flat_multimap(values.begin(), values.end(), comp, alloc) {}
    flat_multimap(initializer_list<value_type> values, const allocator_type &alloc)
        : flat_multimap(values.begin(), values.end(), key_compare(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(initializer_list<value_type> values, const Alloc &alloc)
        : flat_multimap(values.begin(), values.end(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(initializer_list<value_type> values, const key_compare &comp, const Alloc &alloc)
        : flat_multimap(values.begin(), values.end(), comp, alloc) {}
    flat_multimap(sorted_equivalent_t, const key_container_type &keys,
                  const mapped_container_type &mapped, const key_compare &comp = key_compare())
        : base(sorted_unique, keys, mapped, comp) {}
    flat_multimap(sorted_equivalent_t, key_container_type &&keys, mapped_container_type &&mapped,
                  const key_compare &comp = key_compare())
        : base(sorted_unique, std::move(keys), std::move(mapped), comp) {}
    flat_multimap(sorted_equivalent_t, const key_container_type &keys,
                  const mapped_container_type &mapped, const allocator_type &alloc)
        : flat_multimap(sorted_equivalent, keys, mapped, key_compare(), alloc) {}
    flat_multimap(sorted_equivalent_t, const key_container_type &keys,
                  const mapped_container_type &mapped, const key_compare &comp,
                  const allocator_type &alloc)
        : base(sorted_unique, keys, mapped, comp, alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(sorted_equivalent_t, const key_container_type &keys,
                  const mapped_container_type &mapped, const Alloc &alloc)
        : flat_multimap(sorted_equivalent, keys, mapped, key_compare(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(sorted_equivalent_t, const key_container_type &keys,
                  const mapped_container_type &mapped, const key_compare &comp, const Alloc &alloc)
        : base(sorted_unique, keys, mapped, comp, alloc) {}
    flat_multimap(sorted_equivalent_t, key_container_type &&keys, mapped_container_type &&mapped,
                  const allocator_type &alloc)
        : flat_multimap(sorted_equivalent, std::move(keys), std::move(mapped), key_compare(),
                        alloc) {}
    flat_multimap(sorted_equivalent_t, key_container_type &&keys, mapped_container_type &&mapped,
                  const key_compare &comp, const allocator_type &alloc)
        : base(sorted_unique, std::move(keys), std::move(mapped), comp, alloc) {}
    flat_multimap(sorted_equivalent_t, initializer_list<value_type> values,
                  const key_compare &comp = key_compare())
        : flat_multimap(sorted_equivalent, values.begin(), values.end(), comp) {}
    flat_multimap(sorted_equivalent_t, initializer_list<value_type> values, const key_compare &comp,
                  const allocator_type &alloc)
        : flat_multimap(sorted_equivalent, values.begin(), values.end(), comp, alloc) {}
    flat_multimap(sorted_equivalent_t, initializer_list<value_type> values,
                  const allocator_type &alloc)
        : flat_multimap(sorted_equivalent, values.begin(), values.end(), key_compare(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(sorted_equivalent_t, initializer_list<value_type> values, const Alloc &alloc)
        : flat_multimap(sorted_equivalent, values.begin(), values.end(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(sorted_equivalent_t, initializer_list<value_type> values, const key_compare &comp,
                  const Alloc &alloc)
        : flat_multimap(sorted_equivalent, values.begin(), values.end(), comp, alloc) {}
    flat_multimap(key_container_type keys, mapped_container_type mapped,
                  const key_compare &comp = key_compare())
        : base(sorted_equivalent, std::move(keys), std::move(mapped), comp) {}
    flat_multimap(const key_container_type &keys, const mapped_container_type &mapped,
                  const allocator_type &alloc)
        : flat_multimap(keys, mapped, key_compare(), alloc) {}
    flat_multimap(const key_container_type &keys, const mapped_container_type &mapped,
                  const key_compare &comp, const allocator_type &alloc)
        : base(sorted_equivalent, keys, mapped, comp, alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(const key_container_type &keys, const mapped_container_type &mapped,
                  const Alloc &alloc)
        : flat_multimap(keys, mapped, key_compare(), alloc) {}
    template <typename Alloc, enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                                              uses_allocator<mapped_container_type, Alloc>::value,
                                          int> = 0>
    flat_multimap(const key_container_type &keys, const mapped_container_type &mapped,
                  const key_compare &comp, const Alloc &alloc)
        : base(sorted_equivalent, keys, mapped, comp, alloc) {}
#if __cplusplus > 202002L
    template <typename R>
    flat_multimap(from_range_t, R &&range, const key_compare &comp = key_compare()) : base(comp) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_multimap(from_range_t, R &&range, const key_compare &comp, const allocator_type &alloc)
        : base(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
    template <typename R>
    flat_multimap(from_range_t, R &&range, const allocator_type &alloc)
        : flat_multimap(from_range, std::forward<R>(range), key_compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(from_range_t, R &&range, const Alloc &alloc)
        : flat_multimap(from_range, std::forward<R>(range), key_compare(), alloc) {}
    template <typename R, typename Alloc,
              enable_if_t<uses_allocator<key_container_type, Alloc>::value &&
                              uses_allocator<mapped_container_type, Alloc>::value,
                          int> = 0>
    flat_multimap(from_range_t, R &&range, const key_compare &comp, const Alloc &alloc)
        : base(comp, alloc) {
        insert_range(std::forward<R>(range));
    }
    template <typename R> void insert_range(R &&range) {
        for (auto &&value : range)
            this->insert_equivalent(std::forward<decltype(value)>(value).first,
                                    std::forward<decltype(value)>(value).second);
    }
    template <typename R> void insert_range(sorted_equivalent_t, R &&range) {
        insert_range(std::forward<R>(range));
    }
#endif
    iterator insert(const value_type &value) {
        return this->insert_equivalent(value.first, value.second);
    }
    iterator insert(value_type &&value) {
        return this->insert_equivalent(std::move(value.first), std::move(value.second));
    }
    template <typename P> iterator insert(P &&value) {
        return this->insert_equivalent(std::forward<P>(value).first, std::forward<P>(value).second);
    }
    iterator insert(const_iterator, const value_type &value) { return insert(value); }
    iterator insert(const_iterator, value_type &&value) { return insert(std::move(value)); }
    template <typename P> iterator insert(const_iterator, P &&value) {
        return insert(std::forward<P>(value));
    }
    template <typename InputIt> void insert(InputIt first, InputIt last) {
        for (; first != last; ++first)
            insert(*first);
    }
    void insert(initializer_list<value_type> values) { insert(values.begin(), values.end()); }
    void insert(sorted_equivalent_t, initializer_list<value_type> values) {
        insert(values.begin(), values.end());
    }
    template <typename InputIt> void insert(sorted_equivalent_t, InputIt first, InputIt last) {
        insert(first, last);
    }
    template <typename... Args> iterator emplace(Args &&...args) {
        pair<key_type, mapped_type> value(std::forward<Args>(args)...);
        return this->insert_equivalent(std::move(value.first), std::move(value.second));
    }
    template <typename... Args> iterator emplace_hint(const_iterator, Args &&...args) {
        return emplace(std::forward<Args>(args)...);
    }
    mapped_type &operator[](const key_type &) = delete;
    mapped_type &operator[](key_type &&) = delete;
    mapped_type &at(const key_type &) = delete;
    const mapped_type &at(const key_type &) const = delete;
    template <typename... Args>
    pair<iterator, bool> try_emplace(const key_type &, Args &&...) = delete;
    template <typename... Args> pair<iterator, bool> try_emplace(key_type &&, Args &&...) = delete;
    template <typename M> pair<iterator, bool> insert_or_assign(const key_type &, M &&) = delete;
    template <typename M> pair<iterator, bool> insert_or_assign(key_type &&, M &&) = delete;
    void replace(key_container_type keys, mapped_container_type mapped) {
        base candidate(sorted_equivalent, std::move(keys), std::move(mapped), this->key_comp());
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
#endif
            base::swap(candidate);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        } catch (...) {
            this->clear();
            candidate.clear();
            throw;
        }
#endif
    }
    flat_multimap &operator=(initializer_list<value_type> values) {
        this->clear();
        insert(values);
        return *this;
    }
    using base::begin;
    using base::contains;
    using base::end;
    using base::equal_range;
    using base::erase;
    using base::find;
    using base::lower_bound;
    using base::upper_bound;
    size_type count(const key_type &key) const {
        auto r = this->equal_range(key);
        return static_cast<size_type>(r.second - r.first);
    }
    size_type erase(const key_type &key) {
        auto r = this->equal_range(key);
        auto n = static_cast<size_type>(r.second - r.first);
        this->erase(r.first, r.second);
        return n;
    }
};

template <typename KeyContainer, typename Compare = less<typename KeyContainer::value_type>>
flat_set(KeyContainer, Compare = Compare())
    -> flat_set<typename KeyContainer::value_type, Compare, KeyContainer>;
template <typename InputIt, typename Compare = less<typename iterator_traits<InputIt>::value_type>>
flat_set(InputIt, InputIt, Compare = Compare())
    -> flat_set<typename iterator_traits<InputIt>::value_type, Compare>;
template <typename Key, typename Compare = less<Key>>
flat_set(initializer_list<Key>, Compare = Compare()) -> flat_set<Key, Compare>;
template <typename KeyContainer, typename Compare = less<typename KeyContainer::value_type>>
flat_set(sorted_unique_t, KeyContainer, Compare = Compare())
    -> flat_set<typename KeyContainer::value_type, Compare, KeyContainer>;
template <typename InputIt, typename Compare = less<typename iterator_traits<InputIt>::value_type>>
flat_set(sorted_unique_t, InputIt, InputIt, Compare = Compare())
    -> flat_set<typename iterator_traits<InputIt>::value_type, Compare>;
template <typename KeyContainer, typename Compare = less<typename KeyContainer::value_type>>
flat_multiset(KeyContainer, Compare = Compare())
    -> flat_multiset<typename KeyContainer::value_type, Compare, KeyContainer>;
template <typename InputIt, typename Compare = less<typename iterator_traits<InputIt>::value_type>>
flat_multiset(InputIt, InputIt, Compare = Compare())
    -> flat_multiset<typename iterator_traits<InputIt>::value_type, Compare>;
template <typename Key, typename Compare = less<Key>>
flat_multiset(initializer_list<Key>, Compare = Compare()) -> flat_multiset<Key, Compare>;
template <typename KeyContainer, typename MappedContainer,
          typename Compare = less<typename KeyContainer::value_type>>
flat_map(KeyContainer, MappedContainer, Compare = Compare())
    -> flat_map<typename KeyContainer::value_type, typename MappedContainer::value_type, Compare,
                KeyContainer, MappedContainer>;
template <typename InputIt, typename Pair = typename iterator_traits<InputIt>::value_type,
          typename Key = remove_cv_t<remove_reference_t<decltype(declval<Pair>().first)>>,
          typename Mapped = remove_cv_t<remove_reference_t<decltype(declval<Pair>().second)>>,
          typename Compare = less<Key>>
flat_map(InputIt, InputIt, Compare = Compare()) -> flat_map<Key, Mapped, Compare>;
template <typename Key, typename Mapped, typename Compare = less<Key>>
flat_map(initializer_list<pair<Key, Mapped>>, Compare = Compare())
    -> flat_map<Key, Mapped, Compare>;
template <typename KeyContainer, typename MappedContainer,
          typename Compare = less<typename KeyContainer::value_type>>
flat_multimap(KeyContainer, MappedContainer, Compare = Compare())
    -> flat_multimap<typename KeyContainer::value_type, typename MappedContainer::value_type,
                     Compare, KeyContainer, MappedContainer>;
template <typename InputIt, typename Pair = typename iterator_traits<InputIt>::value_type,
          typename Key = remove_cv_t<remove_reference_t<decltype(declval<Pair>().first)>>,
          typename Mapped = remove_cv_t<remove_reference_t<decltype(declval<Pair>().second)>>,
          typename Compare = less<Key>>
flat_multimap(InputIt, InputIt, Compare = Compare()) -> flat_multimap<Key, Mapped, Compare>;
template <typename Key, typename Mapped, typename Compare = less<Key>>
flat_multimap(initializer_list<pair<Key, Mapped>>, Compare = Compare())
    -> flat_multimap<Key, Mapped, Compare>;

#if __cplusplus > 202002L
template <typename R, typename Compare = less<ranges::range_value_t<R>>>
    requires(!uses_allocator_v<vector<ranges::range_value_t<R>>, Compare>)
flat_set(from_range_t, R &&, Compare = Compare()) -> flat_set<ranges::range_value_t<R>, Compare>;
template <typename R, typename Allocator>
    requires uses_allocator_v<vector<ranges::range_value_t<R>>, Allocator>
flat_set(from_range_t, R &&, Allocator) -> flat_set<
    ranges::range_value_t<R>, less<ranges::range_value_t<R>>,
    vector<ranges::range_value_t<R>,
           typename allocator_traits<Allocator>::template rebind_alloc<ranges::range_value_t<R>>>>;
template <typename R, typename Compare, typename Allocator>
    requires uses_allocator_v<vector<ranges::range_value_t<R>>, Allocator>
flat_set(from_range_t, R &&, Compare, Allocator) -> flat_set<
    ranges::range_value_t<R>, Compare,
    vector<ranges::range_value_t<R>,
           typename allocator_traits<Allocator>::template rebind_alloc<ranges::range_value_t<R>>>>;

template <typename R, typename Compare = less<flat_detail::pair_key_t<ranges::range_value_t<R>>>>
    requires(!uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Compare>)
flat_map(from_range_t, R &&, Compare = Compare())
    -> flat_map<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                flat_detail::pair_mapped_t<ranges::range_value_t<R>>, Compare>;
template <typename R, typename Allocator>
    requires(
        uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Allocator> &&
        uses_allocator_v<vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>>, Allocator>)
flat_map(from_range_t, R &&, Allocator)
    -> flat_map<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                less<flat_detail::pair_key_t<ranges::range_value_t<R>>>,
                vector<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                       typename allocator_traits<Allocator>::template rebind_alloc<
                           flat_detail::pair_key_t<ranges::range_value_t<R>>>>,
                vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                       typename allocator_traits<Allocator>::template rebind_alloc<
                           flat_detail::pair_mapped_t<ranges::range_value_t<R>>>>>;
template <typename R, typename Compare, typename Allocator>
    requires(
        uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Allocator> &&
        uses_allocator_v<vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>>, Allocator>)
flat_map(from_range_t, R &&, Compare, Allocator)
    -> flat_map<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                flat_detail::pair_mapped_t<ranges::range_value_t<R>>, Compare,
                vector<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                       typename allocator_traits<Allocator>::template rebind_alloc<
                           flat_detail::pair_key_t<ranges::range_value_t<R>>>>,
                vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                       typename allocator_traits<Allocator>::template rebind_alloc<
                           flat_detail::pair_mapped_t<ranges::range_value_t<R>>>>>;

template <typename R, typename Compare = less<ranges::range_value_t<R>>>
    requires(!uses_allocator_v<vector<ranges::range_value_t<R>>, Compare>)
flat_multiset(from_range_t, R &&, Compare = Compare())
    -> flat_multiset<ranges::range_value_t<R>, Compare>;
template <typename R, typename Allocator>
    requires uses_allocator_v<vector<ranges::range_value_t<R>>, Allocator>
flat_multiset(from_range_t, R &&, Allocator) -> flat_multiset<
    ranges::range_value_t<R>, less<ranges::range_value_t<R>>,
    vector<ranges::range_value_t<R>,
           typename allocator_traits<Allocator>::template rebind_alloc<ranges::range_value_t<R>>>>;
template <typename R, typename Compare, typename Allocator>
    requires uses_allocator_v<vector<ranges::range_value_t<R>>, Allocator>
flat_multiset(from_range_t, R &&, Compare, Allocator) -> flat_multiset<
    ranges::range_value_t<R>, Compare,
    vector<ranges::range_value_t<R>,
           typename allocator_traits<Allocator>::template rebind_alloc<ranges::range_value_t<R>>>>;

template <typename R, typename Compare = less<flat_detail::pair_key_t<ranges::range_value_t<R>>>>
    requires(!uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Compare>)
flat_multimap(from_range_t, R &&, Compare = Compare())
    -> flat_multimap<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                     flat_detail::pair_mapped_t<ranges::range_value_t<R>>, Compare>;
template <typename R, typename Allocator>
    requires(
        uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Allocator> &&
        uses_allocator_v<vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>>, Allocator>)
flat_multimap(from_range_t, R &&, Allocator)
    -> flat_multimap<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                     flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                     less<flat_detail::pair_key_t<ranges::range_value_t<R>>>,
                     vector<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                            typename allocator_traits<Allocator>::template rebind_alloc<
                                flat_detail::pair_key_t<ranges::range_value_t<R>>>>,
                     vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                            typename allocator_traits<Allocator>::template rebind_alloc<
                                flat_detail::pair_mapped_t<ranges::range_value_t<R>>>>>;
template <typename R, typename Compare, typename Allocator>
    requires(
        uses_allocator_v<vector<flat_detail::pair_key_t<ranges::range_value_t<R>>>, Allocator> &&
        uses_allocator_v<vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>>, Allocator>)
flat_multimap(from_range_t, R &&, Compare, Allocator)
    -> flat_multimap<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                     flat_detail::pair_mapped_t<ranges::range_value_t<R>>, Compare,
                     vector<flat_detail::pair_key_t<ranges::range_value_t<R>>,
                            typename allocator_traits<Allocator>::template rebind_alloc<
                                flat_detail::pair_key_t<ranges::range_value_t<R>>>>,
                     vector<flat_detail::pair_mapped_t<ranges::range_value_t<R>>,
                            typename allocator_traits<Allocator>::template rebind_alloc<
                                flat_detail::pair_mapped_t<ranges::range_value_t<R>>>>>;
#endif

template <typename Key, typename Compare, typename Container, typename Pred>
typename flat_set<Key, Compare, Container>::size_type erase_if(flat_set<Key, Compare, Container> &c,
                                                               Pred pred) {
    typename flat_set<Key, Compare, Container>::size_type removed = 0;
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) {
            it = c.erase(it);
            ++removed;
        } else
            ++it;
    }
    return removed;
}
template <typename Key, typename Compare, typename Container, typename Pred>
typename flat_multiset<Key, Compare, Container>::size_type
erase_if(flat_multiset<Key, Compare, Container> &c, Pred pred) {
    typename flat_multiset<Key, Compare, Container>::size_type removed = 0;
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) {
            it = c.erase(it);
            ++removed;
        } else
            ++it;
    }
    return removed;
}
template <typename Key, typename T, typename Compare, typename Keys, typename Values, typename Pred>
typename flat_map<Key, T, Compare, Keys, Values>::size_type
erase_if(flat_map<Key, T, Compare, Keys, Values> &c, Pred pred) {
    typename flat_map<Key, T, Compare, Keys, Values>::size_type removed = 0;
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) {
            it = c.erase(it);
            ++removed;
        } else
            ++it;
    }
    return removed;
}
template <typename Key, typename T, typename Compare, typename Keys, typename Values, typename Pred>
typename flat_multimap<Key, T, Compare, Keys, Values>::size_type
erase_if(flat_multimap<Key, T, Compare, Keys, Values> &c, Pred pred) {
    typename flat_multimap<Key, T, Compare, Keys, Values>::size_type removed = 0;
    for (auto it = c.begin(); it != c.end();) {
        if (pred(*it)) {
            it = c.erase(it);
            ++removed;
        } else
            ++it;
    }
    return removed;
}

template <typename Key, typename Compare, typename Container, typename Allocator>
struct uses_allocator<flat_set<Key, Compare, Container>, Allocator>
    : bool_constant<uses_allocator<Container, Allocator>::value> {};
template <typename Key, typename Compare, typename Container, typename Allocator>
struct uses_allocator<flat_multiset<Key, Compare, Container>, Allocator>
    : bool_constant<uses_allocator<Container, Allocator>::value> {};
template <typename Key, typename T, typename Compare, typename Keys, typename Values,
          typename Allocator>
struct uses_allocator<flat_map<Key, T, Compare, Keys, Values>, Allocator>
    : bool_constant<uses_allocator<Keys, Allocator>::value &&
                    uses_allocator<Values, Allocator>::value> {};
template <typename Key, typename T, typename Compare, typename Keys, typename Values,
          typename Allocator>
struct uses_allocator<flat_multimap<Key, T, Compare, Keys, Values>, Allocator>
    : bool_constant<uses_allocator<Keys, Allocator>::value &&
                    uses_allocator<Values, Allocator>::value> {};

} // namespace std
#endif

#endif
