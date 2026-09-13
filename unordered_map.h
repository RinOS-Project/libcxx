/*
 * RinOS C++ <unordered_map> ✿
 * 完全なハッシュマップ実装
 */

#ifndef RINCXX_UNORDERED_MAP_H
#define RINCXX_UNORDERED_MAP_H

#include "rincxx.h"
#include "vector.h"
#include "functional.h"
#include "utility.h"
#include "iterator.h"
#include "memory.h"
#include "tuple.h"
#include "stdexcept.h"
#if __cplusplus > 202002L
#include "ranges.h"
#endif

/* P2363 heterogeneous insertion is a C++26 facility.  Keep the preview
 * opt-in consistent with the ordered map implementation; older dialects
 * must not accidentally expose these overloads. */
#if __cplusplus > 202302L || defined(RIN_ENABLE_CXX26_ASSOCIATIVE_INSERTION)
#define RIN_UNORDERED_HAS_HETERO_INSERTION 1
#endif

#ifdef __cplusplus

namespace std {

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
class unordered_multimap;

namespace unordered_map_detail {

template<typename Hash, typename Equal, typename Key, typename K,
         typename = void>
struct heterogeneous_lookup : false_type {};

template<typename Hash, typename Equal, typename Key, typename K>
struct heterogeneous_lookup<Hash, Equal, Key, K, void_t<
    typename Hash::is_transparent,
    typename Equal::is_transparent,
    decltype(declval<const Hash&>()(declval<const K&>())),
    decltype(declval<const Equal&>()(
        declval<const Key&>(), declval<const K&>())),
    decltype(declval<const Equal&>()(
        declval<const K&>(), declval<const Key&>()))>> : true_type {};

/* Avoid qualified std::swap ambiguity when Rin and a hosted standard
 * library are both visible.  Publication paths use this three-move exchange
 * so their noexcept result follows the actual member operations. */
template<typename T>
inline void exchange(T& left, T& right) noexcept(
    noexcept(T(std::move(left))) &&
    noexcept(left = std::move(right)) &&
    noexcept(right = std::move(left))) {
    if (&left == &right) return;
    T temporary(std::move(left));
    left = std::move(right);
    right = std::move(temporary);
}

[[noreturn]] inline void allocation_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_alloc();
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void length_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw length_error("unordered_map::swap exceeds max_size");
#else
    __builtin_trap();
#endif
}

[[noreturn]] inline void load_factor_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw invalid_argument("unordered_map::max_load_factor requires a positive value");
#else
    __builtin_trap();
#endif
}

template<typename T>
inline T* pointer_address(T* pointer) noexcept {
    return pointer;
}

#if __cplusplus >= 202002L
template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(std::to_address(pointer)) {
    return std::to_address(pointer);
}
#else
template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_traits<Pointer>::to_address(pointer)) {
    return pointer_traits<Pointer>::to_address(pointer);
}

template<typename Pointer>
inline auto pointer_address(const Pointer& pointer) noexcept
    -> decltype(pointer_address(pointer.operator->())) {
    return pointer_address(pointer.operator->());
}
#endif

} /* namespace unordered_map_detail */

/* ═══════════════════════════════════════════════════════════════
 * unordered_map - ハッシュテーブルベースの連想配列
 * ═══════════════════════════════════════════════════════════════*/

template<
    typename Key,
    typename T,
    typename Hash = hash<Key>,
    typename KeyEqual = equal_to<Key>,
    typename Allocator = allocator<pair<const Key, T>>
>
class unordered_map {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = pair<const Key, T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

private:
    [[noreturn]] static void at_failure() {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        throw out_of_range("unordered_map::at: key not found");
#else
        __builtin_trap();
#endif
    }

public:
    
private:
    /* バケットエントリ - インプレース構築対応 */
    struct Node {
        /* Use aligned storage to allow in-place construction */
        alignas(value_type) unsigned char storage[sizeof(value_type)];
        Node* next;
        size_t hash_value;

        value_type& data() { return *reinterpret_cast<value_type*>(storage); }
        const value_type& data() const { return *reinterpret_cast<const value_type*>(storage); }

        template<typename K, typename V>
        Node(K&& k, V&& v, size_t h) : next(nullptr), hash_value(h) {
            ::new (storage) value_type(std::forward<K>(k), std::forward<V>(v));
        }

        Node(const value_type& val, size_t h) : next(nullptr), hash_value(h) {
            ::new (storage) value_type(val);
        }

        Node(value_type&& val, size_t h) : next(nullptr), hash_value(h) {
            ::new (storage) value_type(std::move(val));
        }

        /* try_emplace用タグ */
        struct emplace_tag {};

        /* try_emplace用: keyとvalueのコンストラクタ引数を別々に受け取る */
        template<typename K, typename... Args>
        Node(emplace_tag, K&& k, size_t h, Args&&... args) : next(nullptr), hash_value(h) {
            ::new (storage) value_type(
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<K>(k)),
                std::forward_as_tuple(std::forward<Args>(args)...)
            );
        }

        ~Node() {
            data().~value_type();
        }
    };

    using node_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Node>;
    using node_allocator_traits = allocator_traits<node_allocator_type>;
    using node_pointer = typename node_allocator_traits::pointer;

public:
    /* C++17 node handle.  Keeping the original node preserves move-only
     * mapped values during same-allocator transfers; a different allocator
     * is handled by the insertion path below. */
    class node_type {
        Node* m_node;
        allocator_type m_alloc;

        node_type(Node* node, const allocator_type& alloc)
            : m_node(node), m_alloc(alloc) {}
        friend class unordered_map;

        void reset() noexcept {
            if (!m_node) return;
            node_allocator_type node_alloc(m_alloc);
            node_allocator_traits::destroy(node_alloc, m_node);
            node_allocator_traits::deallocate(
                node_alloc,
                pointer_traits<node_pointer>::pointer_to(*m_node), 1);
            m_node = nullptr;
        }

    public:
        node_type() : m_node(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_node(other.m_node), m_alloc(std::move(other.m_alloc)) {
            other.m_node = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_node = other.m_node;
                m_alloc = std::move(other.m_alloc);
                other.m_node = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_node == nullptr; }
        explicit operator bool() const noexcept { return !empty(); }
        key_type& key() const { return const_cast<key_type&>(m_node->data().first); }
        mapped_type& mapped() const { return m_node->data().second; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
    };

private:

    allocator_type m_alloc;
    vector<Node*> m_buckets;
    size_type m_size;
    float m_max_load_factor;
    hasher m_hash;
    key_equal m_equal;
    
    static constexpr size_type INITIAL_BUCKET_COUNT = 16;

    template<typename... Args>
    Node* allocate_node(Args&&... args) {
        node_allocator_type node_alloc(m_alloc);
        node_pointer allocation = node_allocator_traits::allocate(node_alloc, 1);
        Node* node = unordered_map_detail::pointer_address(allocation);
        if (allocation == node_pointer() || !node) {
            if (allocation != node_pointer())
                node_allocator_traits::deallocate(node_alloc, allocation, 1);
            unordered_map_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            node_allocator_traits::construct(
                node_alloc, node, std::forward<Args>(args)...);
        } catch (...) {
            node_allocator_traits::deallocate(node_alloc, allocation, 1);
            throw;
        }
#else
        node_allocator_traits::construct(
            node_alloc, node, std::forward<Args>(args)...);
#endif
        return node;
    }

    void destroy_node(Node* node) noexcept {
        node_allocator_type node_alloc(m_alloc);
        node_allocator_traits::destroy(node_alloc, node);
        node_allocator_traits::deallocate(
            node_alloc, pointer_traits<node_pointer>::pointer_to(*node), 1);
    }

    void relocate_values(unordered_map& source, unordered_map& target,
                         true_type) {
        for (const auto& value : source)
            target.insert(value);
    }

    void relocate_values(unordered_map& source, unordered_map& target,
                         false_type) {
        for (size_type index = 0; index < source.m_buckets.size(); ++index) {
            for (Node* node = source.m_buckets[index]; node; node = node->next) {
                /* Iterators intentionally expose const map values.  Relocation
                 * of a non-copyable mapped object must therefore use the
                 * mutable source node while the replacement tree is private. */
                target.insert(value_type(node->data().first,
                                         std::move(node->data().second)));
            }
        }
    }

    void swap_unequal(unordered_map& other) {
        if (other.m_size > max_size() || m_size > other.max_size())
            unordered_map_detail::length_failure();
        const size_type left_buckets = other.m_buckets.size() ?
            other.m_buckets.size() : INITIAL_BUCKET_COUNT;
        const size_type right_buckets = m_buckets.size() ?
            m_buckets.size() : INITIAL_BUCKET_COUNT;
        unordered_map left(left_buckets, other.m_hash, other.m_equal, m_alloc);
        unordered_map right(right_buckets, m_hash, m_equal, other.m_alloc);
        left.m_max_load_factor = other.m_max_load_factor;
        right.m_max_load_factor = m_max_load_factor;
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());
        swap(left);
        other.swap(right);
    }

    void copy_assign_allocator(const allocator_type& other, true_type) {
        m_alloc = other;
    }

    void copy_assign_allocator(const allocator_type&, false_type) {}

    void move_assign_allocator(allocator_type&& other, true_type) {
        m_alloc = std::move(other);
    }

    void move_assign_allocator(allocator_type&&, false_type) {}

    void commit_copy_assignment(unordered_map& candidate, true_type) {
        /* The candidate is complete before the destination is touched.  The
         * old nodes must be released with the old allocator, then ownership
         * of the candidate bucket vector can be published under the source
         * allocator selected by POC copy assignment. */
        m_hash = candidate.m_hash;
        m_equal = candidate.m_equal;
        m_max_load_factor = candidate.m_max_load_factor;
        clear();
        m_alloc = candidate.m_alloc;
        m_buckets = std::move(candidate.m_buckets);
        m_size = candidate.m_size;
        candidate.m_size = 0;
    }

    void commit_copy_assignment(unordered_map& candidate, false_type) {
        /* The candidate was built with this allocator, so non-propagating
         * swap keeps every node paired with its owner. */
        swap(candidate);
    }
    
    size_type bucket_index(size_t hash) const {
        return hash % m_buckets.size();
    }

    bool locate_node(const Node* target, size_type& bucket) const noexcept {
        if (!target) return false;
        for (size_type i = 0; i < m_buckets.size(); ++i) {
            for (const Node* node = m_buckets[i]; node; node = node->next) {
                if (node == target) {
                    bucket = i;
                    return true;
                }
            }
        }
        return false;
    }

    Node* insert_node_from_handle(node_type& node, true_type) {
        Node* replacement = allocate_node(node.m_node->data(),
                                          node.m_node->hash_value);
        node.reset();
        return replacement;
    }

    Node* insert_node_from_handle(node_type& node, false_type) {
        Node* replacement = allocate_node(
            value_type(node.m_node->data().first,
                       std::move(node.m_node->data().second)),
            node.m_node->hash_value);
        node.reset();
        return replacement;
    }

    void ensure_bucket_storage() {
        if (!m_buckets.empty()) return;
        m_buckets.resize(INITIAL_BUCKET_COUNT, nullptr);
    }
    
    void rehash_if_needed() {
        if (load_factor() > m_max_load_factor) {
            rehash(m_buckets.size() * 2);
        }
    }

#if __cplusplus > 202002L
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        const size_type candidate_buckets = m_buckets.size() ?
            m_buckets.size() : INITIAL_BUCKET_COUNT;
        unordered_map candidate(candidate_buckets, m_hash, m_equal, m_alloc);
        candidate.m_max_load_factor = m_max_load_factor;
        for (const auto& value : *this) candidate.insert(value);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) candidate.insert(*first);

        clear();
        m_buckets = std::move(candidate.m_buckets);
        m_size = candidate.m_size;
        candidate.m_size = 0;
    }

    template<typename R>
    void insert_range_impl(R&& range, false_type) {
        /* Moving existing mapped values into a private table gives the
         * move-only path a valid basic guarantee: a throwing hash, equality,
         * allocation, or mapped move leaves a valid destination, although
         * values already moved from the source may be unspecified. */
        const size_type candidate_buckets = m_buckets.size() ?
            m_buckets.size() : INITIAL_BUCKET_COUNT;
        unordered_map candidate(candidate_buckets, m_hash, m_equal, m_alloc);
        candidate.m_max_load_factor = m_max_load_factor;
        for (size_type index = 0; index < m_buckets.size(); ++index) {
            for (Node* node = m_buckets[index]; node; node = node->next) {
                candidate.insert(value_type(node->data().first,
                                            std::move(node->data().second)));
            }
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) candidate.insert(std::move(*first));

        clear();
        m_buckets = std::move(candidate.m_buckets);
        m_size = candidate.m_size;
        candidate.m_size = 0;
    }
#endif
    
public:
    /* イテレータ */
    class iterator {
        friend class unordered_map;
        unordered_map* m_map;
        size_type m_bucket;
        Node* m_node;
        
        void advance_to_valid() {
            while (m_node == nullptr && m_bucket < m_map->m_buckets.size()) {
                ++m_bucket;
                if (m_bucket < m_map->m_buckets.size()) {
                    m_node = m_map->m_buckets[m_bucket];
                }
            }
        }
        
    public:
        using iterator_category = forward_iterator_tag;
        using value_type = unordered_map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        
        iterator() : m_map(nullptr), m_bucket(0), m_node(nullptr) {}
        iterator(unordered_map* map, size_type bucket, Node* node)
            : m_map(map), m_bucket(bucket), m_node(node) {
            if (m_node == nullptr && m_map) advance_to_valid();
        }
        
        reference operator*() const { return m_node->data(); }
        pointer operator->() const { return &m_node->data(); }
        
        iterator& operator++() {
            if (m_node) {
                m_node = m_node->next;
                if (m_node == nullptr) {
                    ++m_bucket;
                    if (m_bucket < m_map->m_buckets.size()) {
                        m_node = m_map->m_buckets[m_bucket];
                    }
                    advance_to_valid();
                }
            }
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const iterator& other) const {
            return m_map == other.m_map && m_bucket == other.m_bucket && m_node == other.m_node;
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };
    
    class const_iterator {
        friend class unordered_map;
        const unordered_map* m_map;
        size_type m_bucket;
        const Node* m_node;
        
        void advance_to_valid() {
            while (m_node == nullptr && m_bucket < m_map->m_buckets.size()) {
                ++m_bucket;
                if (m_bucket < m_map->m_buckets.size()) {
                    m_node = m_map->m_buckets[m_bucket];
                }
            }
        }
        
    public:
        using iterator_category = forward_iterator_tag;
        using value_type = const unordered_map::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        
        const_iterator() : m_map(nullptr), m_bucket(0), m_node(nullptr) {}
        const_iterator(const unordered_map* map, size_type bucket, const Node* node)
            : m_map(map), m_bucket(bucket), m_node(node) {
            if (m_node == nullptr && m_map) advance_to_valid();
        }
        const_iterator(const iterator& it)
            : m_map(it.m_map), m_bucket(it.m_bucket), m_node(it.m_node) {}
        
        reference operator*() const { return m_node->data(); }
        pointer operator->() const { return &m_node->data(); }
        
        const_iterator& operator++() {
            if (m_node) {
                m_node = m_node->next;
                if (m_node == nullptr) {
                    ++m_bucket;
                    if (m_bucket < m_map->m_buckets.size()) {
                        m_node = m_map->m_buckets[m_bucket];
                    }
                    advance_to_valid();
                }
            }
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            return m_map == other.m_map && m_bucket == other.m_bucket && m_node == other.m_node;
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

        friend bool operator==(const iterator& left,
                              const const_iterator& right) {
            return left.m_map == right.m_map &&
                   left.m_bucket == right.m_bucket &&
                   left.m_node == right.m_node;
        }
        friend bool operator==(const const_iterator& left,
                              const iterator& right) {
            return right == left;
        }
        friend bool operator!=(const iterator& left,
                              const const_iterator& right) {
            return !(left == right);
        }
        friend bool operator!=(const const_iterator& left,
                              const iterator& right) {
            return !(left == right);
        }
    };
    
    using local_iterator = iterator;
    using const_local_iterator = const_iterator;

    struct insert_return_type {
        iterator position;
        bool inserted;
        node_type node;
    };
    
    /* コンストラクタ・デストラクタ */
    explicit unordered_map(size_type bucket_count = INITIAL_BUCKET_COUNT,
                          const hasher& hash = hasher(),
                          const key_equal& equal = key_equal())
        : m_alloc(), m_buckets(bucket_count, nullptr), m_size(0), m_max_load_factor(1.0f),
          m_hash(hash), m_equal(equal) {}

    /* アロケータ付きコンストラクタ */
    unordered_map(size_type bucket_count,
                  const hasher& hash,
                  const key_equal& equal,
                  const allocator_type& alloc)
        : m_alloc(alloc), m_buckets(bucket_count, nullptr), m_size(0), m_max_load_factor(1.0f),
          m_hash(hash), m_equal(equal) {}

    explicit unordered_map(const allocator_type& alloc)
        : unordered_map(INITIAL_BUCKET_COUNT, hasher(), key_equal(), alloc) {}

    template<typename InputIt,
             typename enable_if<!is_integral<InputIt>::value, int>::type = 0>
    unordered_map(InputIt first, InputIt last,
                  size_type bucket_count = INITIAL_BUCKET_COUNT,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : unordered_map(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    unordered_map(std::initializer_list<value_type> init,
                  size_type bucket_count = INITIAL_BUCKET_COUNT,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : unordered_map(bucket_count, hash, equal, alloc) {
        for (const auto& val : init) {
            insert(val);
        }
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    unordered_map(from_range_t, R&& range,
                  size_type bucket_count = INITIAL_BUCKET_COUNT,
                  const hasher& hash = hasher(),
                  const key_equal& equal = key_equal(),
                  const allocator_type& alloc = allocator_type())
        : unordered_map(bucket_count, hash, equal, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif
    
    unordered_map(const unordered_map& other)
        : m_alloc(allocator_traits<allocator_type>::select_on_container_copy_construction(other.m_alloc)),
          m_buckets(other.m_buckets.size(), nullptr), m_size(0),
          m_max_load_factor(other.m_max_load_factor),
          m_hash(other.m_hash), m_equal(other.m_equal) {
        for (const auto& pair : other) {
            insert(pair);
        }
    }
    
    unordered_map(unordered_map&& other) noexcept
        : m_alloc(std::move(other.m_alloc)),
          m_buckets(std::move(other.m_buckets)), m_size(other.m_size),
          m_max_load_factor(other.m_max_load_factor),
          m_hash(std::move(other.m_hash)), m_equal(std::move(other.m_equal)) {
        other.m_size = 0;
    }
    
    ~unordered_map() {
        clear();
    }
    
    unordered_map& operator=(const unordered_map& other) {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment;
            unordered_map candidate(
                other.m_buckets.size(), other.m_hash, other.m_equal,
                propagate::value ? other.m_alloc : m_alloc);
            candidate.m_max_load_factor = other.m_max_load_factor;
            for (const auto& pair : other) candidate.insert(pair);
            commit_copy_assignment(candidate, propagate());
        }
        return *this;
    }
    
    unordered_map& operator=(unordered_map&& other) noexcept {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_move_assignment;
            if (propagate::value || m_alloc == other.m_alloc) {
                clear();
                move_assign_allocator(std::move(other.m_alloc), propagate());
                m_buckets = std::move(other.m_buckets);
                m_size = other.m_size;
                m_max_load_factor = other.m_max_load_factor;
                m_hash = std::move(other.m_hash);
                m_equal = std::move(other.m_equal);
                other.m_size = 0;
            } else {
                clear();
                m_buckets.resize(other.m_buckets.size(), nullptr);
                for (size_type i = 0; i < m_buckets.size(); ++i) m_buckets[i] = nullptr;
                m_size = 0;
                m_max_load_factor = other.m_max_load_factor;
                m_hash = other.m_hash;
                m_equal = other.m_equal;
                for (size_type index = 0; index < other.m_buckets.size(); ++index) {
                    for (Node* node = other.m_buckets[index]; node;
                         node = node->next) {
                        insert(value_type(node->data().first,
                                          std::move(node->data().second)));
                    }
                }
                other.clear();
            }
        }
        return *this;
    }
    
    /* イテレータ */
    iterator begin() noexcept {
        for (size_type i = 0; i < m_buckets.size(); ++i) {
            if (m_buckets[i]) return iterator(this, i, m_buckets[i]);
        }
        return end();
    }
    
    const_iterator begin() const noexcept {
        for (size_type i = 0; i < m_buckets.size(); ++i) {
            if (m_buckets[i]) return const_iterator(this, i, m_buckets[i]);
        }
        return end();
    }
    
    const_iterator cbegin() const noexcept { return begin(); }
    
    iterator end() noexcept {
        return iterator(this, m_buckets.size(), nullptr);
    }
    
    const_iterator end() const noexcept {
        return const_iterator(this, m_buckets.size(), nullptr);
    }
    
    const_iterator cend() const noexcept { return end(); }
    
    /* 容量 */
    bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept {
        return allocator_traits<allocator_type>::max_size(m_alloc);
    }
    
    /* 要素アクセス */
    mapped_type& operator[](const key_type& key) {
        size_t h = m_hash(key);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);
        
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        
        /* 新規挿入 */
        rehash_if_needed();
        idx = bucket_index(h);
        
        Node* new_node = allocate_node(key, mapped_type{}, h);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return new_node->data().second;
    }
    
    mapped_type& operator[](key_type&& key) {
        size_t h = m_hash(key);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);
        
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        
        rehash_if_needed();
        idx = bucket_index(h);
        
        Node* new_node = allocate_node(std::move(key), mapped_type{}, h);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return new_node->data().second;
    }

#if defined(RIN_UNORDERED_HAS_HETERO_INSERTION)
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value &&
                 is_constructible<mapped_type>::value>>
    mapped_type& operator[](K&& key) {
        /* Reuse try_emplace so an existing heterogeneous key never creates a
         * temporary mapped value and a missing key is published once. */
        return try_emplace(std::forward<K>(key)).first->second;
    }
#endif
    
    mapped_type& at(const key_type& key) {
        if (m_buckets.empty()) at_failure();
        size_t h = m_hash(key);
        size_type idx = bucket_index(h);
        
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        /* 見つからない場合のエラー処理 */
        at_failure();
    }
    
    const mapped_type& at(const key_type& key) const {
        if (m_buckets.empty()) at_failure();
        size_t h = m_hash(key);
        size_type idx = bucket_index(h);
        
        for (const Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        at_failure();
    }

#if defined(RIN_UNORDERED_HAS_HETERO_INSERTION)
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value>>
    mapped_type& at(K&& key) {
        if (m_buckets.empty()) at_failure();
        const size_t h = m_hash(key);
        const size_type idx = bucket_index(h);
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h &&
                m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        at_failure();
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value>>
    const mapped_type& at(K&& key) const {
        if (m_buckets.empty()) at_failure();
        const size_t h = m_hash(key);
        const size_type idx = bucket_index(h);
        for (const Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h &&
                m_equal(node->data().first, key)) {
                return node->data().second;
            }
        }
        at_failure();
    }
#endif
    
    /* 変更 */
    void clear() noexcept {
        for (size_type i = 0; i < m_buckets.size(); ++i) {
            Node* node = m_buckets[i];
            while (node) {
                Node* next = node->next;
                destroy_node(node);
                node = next;
            }
            m_buckets[i] = nullptr;
        }
        m_size = 0;
    }
    
    pair<iterator, bool> insert(const value_type& value) {
        size_t h = m_hash(value.first);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);

        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, value.first)) {
                return {iterator(this, idx, node), false};
            }
        }

        rehash_if_needed();
        idx = bucket_index(h);

        Node* new_node = allocate_node(value, h);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return {iterator(this, idx, new_node), true};
    }

    pair<iterator, bool> insert(value_type&& value) {
        size_t h = m_hash(value.first);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);

        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, value.first)) {
                return {iterator(this, idx, node), false};
            }
        }

        rehash_if_needed();
        idx = bucket_index(h);

        Node* new_node = allocate_node(std::move(value), h);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return {iterator(this, idx, new_node), true};
    }

    template<typename P, typename = typename std::enable_if<!std::is_same<typename std::decay<P>::type, value_type>::value>::type>
    pair<iterator, bool> insert(P&& value) {
        return emplace(std::forward<P>(value));
    }

    /* Hint-based insert - hint is ignored for unordered containers */
    iterator insert(const_iterator /* hint */, const value_type& value) {
        return insert(value).first;
    }

    iterator insert(const_iterator /* hint */, value_type&& value) {
        return insert(std::move(value)).first;
    }

    template<typename P, typename = typename std::enable_if<!std::is_same<typename std::decay<P>::type, value_type>::value>::type>
    iterator insert(const_iterator /* hint */, P&& value) {
        return insert(std::forward<P>(value)).first;
    }

    template<typename... Args>
    pair<iterator, bool> emplace(Args&&... args) {
        /* 一時的にpairを構築してmoveでinsert */
        value_type val(std::forward<Args>(args)...);
        return insert(std::move(val));
    }

    template<typename... Args>
    iterator emplace_hint(const_iterator, Args&&... args) {
        return emplace(std::forward<Args>(args)...).first;
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) insert(*first);
    }

    void insert(initializer_list<value_type> init) {
        insert(init.begin(), init.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          is_copy_constructible<value_type>());
    }
#endif
    
    template<typename... Args>
    pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        size_t h = m_hash(key);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);
        
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return {iterator(this, idx, node), false};
            }
        }
        
        rehash_if_needed();
        idx = bucket_index(h);
        
        Node* new_node = allocate_node(typename Node::emplace_tag{}, key, h,
                                       std::forward<Args>(args)...);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return {iterator(this, idx, new_node), true};
    }

#if defined(RIN_UNORDERED_HAS_HETERO_INSERTION)
    template<typename K, typename... Args,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value>>
    pair<iterator, bool> try_emplace(K&& key, Args&&... args) {
        size_t h = m_hash(key);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);

        /* Probe before constructing the mapped value.  A duplicate
         * heterogeneous key therefore has no observable mapped construction
         * and no mutation of the table. */
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h &&
                m_equal(node->data().first, key)) {
                return {iterator(this, idx, node), false};
            }
        }

        rehash_if_needed();
        idx = bucket_index(h);
        Node* new_node = allocate_node(typename Node::emplace_tag{},
                                       std::forward<K>(key), h,
                                       std::forward<Args>(args)...);
        new_node->next = m_buckets[idx];
        m_buckets[idx] = new_node;
        ++m_size;
        return {iterator(this, idx, new_node), true};
    }

    template<typename K, typename... Args,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value>>
    iterator try_emplace(const_iterator /* hint */, K&& key,
                         Args&&... args) {
        return try_emplace(std::forward<K>(key),
                           std::forward<Args>(args)...).first;
    }
#endif

    /* insert_or_assign - C++17: 挿入または代入 */
    template<typename M>
    pair<iterator, bool> insert_or_assign(const key_type& key, M&& obj) {
        auto it = find(key);
        if (it != end()) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        return emplace(key, std::forward<M>(obj));
    }

    template<typename M>
    pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
        auto it = find(key);
        if (it != end()) {
            it->second = std::forward<M>(obj);
            return {it, false};
        }
        return emplace(std::move(key), std::forward<M>(obj));
    }

#if defined(RIN_UNORDERED_HAS_HETERO_INSERTION)
    template<typename K, typename M,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value &&
                 is_constructible<mapped_type, M&&>::value &&
                 is_assignable<mapped_type&, M&&>::value>>
    pair<iterator, bool> insert_or_assign(K&& key, M&& obj) {
        if (!m_buckets.empty()) {
            const size_t h = m_hash(key);
            const size_type idx = bucket_index(h);
            for (Node* node = m_buckets[idx]; node; node = node->next) {
                if (node->hash_value == h &&
                    m_equal(node->data().first, key)) {
                    node->data().second = std::forward<M>(obj);
                    return {iterator(this, idx, node), false};
                }
            }
        }
        return try_emplace(std::forward<K>(key),
                           std::forward<M>(obj));
    }

    template<typename K, typename M,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value &&
                 is_constructible<key_type, K&&>::value &&
                 is_constructible<mapped_type, M&&>::value &&
                 is_assignable<mapped_type&, M&&>::value>>
    iterator insert_or_assign(const_iterator /* hint */, K&& key, M&& obj) {
        return insert_or_assign(std::forward<K>(key),
                                std::forward<M>(obj)).first;
    }
#endif

    iterator erase(const_iterator pos) {
        if (!pos.m_node && pos.m_map != this) return end();
        size_type idx = 0;
        Node* target = const_cast<Node*>(pos.m_node);
        if (!locate_node(target, idx)) return end();
        
        if (m_buckets[idx] == target) {
            m_buckets[idx] = target->next;
        } else {
            Node* prev = m_buckets[idx];
            while (prev && prev->next != target) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = target->next;
            }
        }
        
        destroy_node(target);
        --m_size;
        if (m_buckets[idx]) return iterator(this, idx, m_buckets[idx]);
        for (size_type next = idx + 1; next < m_buckets.size(); ++next)
            if (m_buckets[next]) return iterator(this, next, m_buckets[next]);
        return end();
    }

    insert_return_type insert(node_type&& node) {
        if (node.empty()) return {end(), false, node_type()};
        const size_t h = m_hash(node.m_node->data().first);
        ensure_bucket_storage();
        size_type idx = bucket_index(h);
        for (Node* current = m_buckets[idx]; current; current = current->next) {
            if (current->hash_value == h &&
                m_equal(current->data().first, node.m_node->data().first)) {
                return {iterator(this, idx, current), false, std::move(node)};
            }
        }

        rehash_if_needed();
        idx = bucket_index(h);
        Node* adopted = nullptr;
        if (m_alloc == node.m_alloc) {
            adopted = node.m_node;
            node.m_node = nullptr;
        } else {
            /* Copyable values preserve the handle on construction failure;
             * move-only values use the standard basic-guarantee fallback. */
            adopted = insert_node_from_handle(node,
                is_copy_constructible<value_type>());
        }
        adopted->hash_value = h;
        adopted->next = m_buckets[idx];
        m_buckets[idx] = adopted;
        ++m_size;
        return {iterator(this, idx, adopted), true, node_type()};
    }

    iterator insert(const_iterator /* hint */, node_type&& node) {
        return insert(std::move(node)).position;
    }

    node_type extract(const_iterator position) {
        if ((!position.m_node && position.m_map != this) || !position.m_node)
            return node_type();
        size_type idx = 0;
        Node* target = const_cast<Node*>(position.m_node);
        if (!locate_node(target, idx)) return node_type();
        if (m_buckets[idx] == target) {
            m_buckets[idx] = target->next;
        } else {
            Node* previous = m_buckets[idx];
            while (previous && previous->next != target) previous = previous->next;
            if (!previous) return node_type();
            previous->next = target->next;
        }
        target->next = nullptr;
        --m_size;
        return node_type(target, m_alloc);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void merge(unordered_map& source) {
        if (this == &source) return;
        for (size_type index = 0; index < source.m_buckets.size(); ++index) {
            Node* current = source.m_buckets[index];
            while (current) {
                Node* next = current->next;
                if (find(current->data().first) == end()) {
                    node_type detached = source.extract(
                        typename unordered_map::const_iterator(
                            &source, index, current));
                    insert(std::move(detached));
                }
                current = next;
            }
        }
    }

    void merge(unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& source);

    template<typename H2, typename E2, typename A2>
    void merge(unordered_map<Key, T, H2, E2, A2>& source);

    template<typename H2, typename E2, typename A2>
    void merge(unordered_multimap<Key, T, H2, E2, A2>& source);
    
    size_type erase(const key_type& key) {
        iterator it = find(key);
        if (it == end()) return 0;
        erase(it);
        return 1;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.m_node && first.m_map != this) ||
            (!last.m_node && last.m_map != this)) return end();
        if (first.m_node && !locate_node(first.m_node, first.m_bucket)) return end();
        if (last.m_node && !locate_node(last.m_node, last.m_bucket)) return end();
        const_iterator boundary(this, last.m_bucket, last.m_node);
        while (first.m_node != boundary.m_node) first = erase(first);
        return iterator(this, boundary.m_bucket,
                        const_cast<Node*>(boundary.m_node));
    }
    
    /* 検索 */
    iterator find(const key_type& key) {
        if (m_buckets.empty()) return end();
        size_t h = m_hash(key);
        size_type idx = bucket_index(h);
        
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return iterator(this, idx, node);
            }
        }
        return end();
    }
    
    const_iterator find(const key_type& key) const {
        if (m_buckets.empty()) return end();
        size_t h = m_hash(key);
        size_type idx = bucket_index(h);
        
        for (const Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h && m_equal(node->data().first, key)) {
                return const_iterator(this, idx, node);
            }
        }
        return end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    iterator find(const K& key) {
        if (m_buckets.empty()) return end();
        const size_t h = m_hash(key);
        const size_type idx = bucket_index(h);
        for (Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h &&
                m_equal(node->data().first, key)) {
                return iterator(this, idx, node);
            }
        }
        return end();
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    const_iterator find(const K& key) const {
        if (m_buckets.empty()) return end();
        const size_t h = m_hash(key);
        const size_type idx = bucket_index(h);
        for (const Node* node = m_buckets[idx]; node; node = node->next) {
            if (node->hash_value == h &&
                m_equal(node->data().first, key)) {
                return const_iterator(this, idx, node);
            }
        }
        return end();
    }
#endif
    
    size_type count(const key_type& key) const {
        return find(key) != end() ? 1 : 0;
    }
    
    bool contains(const key_type& key) const {
        return find(key) != end();
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type count(const K& key) const {
        return find(key) != end() ? 1 : 0;
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    bool contains(const K& key) const {
        return find(key) != end();
    }
#endif

    /* Unique-key maps still expose the standard equal_range observer. */
    pair<iterator, iterator> equal_range(const key_type& key) {
        iterator first = find(key);
        if (first == end()) return make_pair(end(), end());
        iterator last = first;
        ++last;
        return make_pair(first, last);
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        const_iterator first = find(key);
        if (first == end()) return make_pair(end(), end());
        const_iterator last = first;
        ++last;
        return make_pair(first, last);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<iterator, iterator> equal_range(const K& key) {
        iterator first = find(key);
        if (first == end()) return make_pair(end(), end());
        iterator last = first;
        ++last;
        return make_pair(first, last);
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        const_iterator first = find(key);
        if (first == end()) return make_pair(end(), end());
        const_iterator last = first;
        ++last;
        return make_pair(first, last);
    }
#endif
    
    /* バケット操作 */
    size_type bucket_count() const noexcept { return m_buckets.size(); }
    size_type max_bucket_count() const noexcept { return max_size(); }
    size_type bucket_size(size_type n) const {
        size_type count = 0;
        for (const Node* node = m_buckets[n]; node; node = node->next) {
            ++count;
        }
        return count;
    }
    size_type bucket(const key_type& key) const {
        if (m_buckets.empty()) return 0;
        return bucket_index(m_hash(key));
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type bucket(const K& key) const {
        if (m_buckets.empty()) return 0;
        return bucket_index(m_hash(key));
    }
#endif
    
    /* ロードファクター */
    float load_factor() const noexcept {
        return m_buckets.size() > 0 ? static_cast<float>(m_size) / m_buckets.size() : 0.0f;
    }
    
    float max_load_factor() const noexcept { return m_max_load_factor; }
    void max_load_factor(float ml) {
        if (!(ml > 0.0f))
            unordered_map_detail::load_factor_failure();
        m_max_load_factor = ml;
    }
    
    void rehash(size_type count) {
        if (count < m_size / m_max_load_factor) {
            count = static_cast<size_type>(m_size / m_max_load_factor) + 1;
        }
        if (count > max_bucket_count())
            unordered_map_detail::length_failure();
        
        vector<Node*> new_buckets(count, nullptr);
        
        for (size_type i = 0; i < m_buckets.size(); ++i) {
            Node* node = m_buckets[i];
            while (node) {
                Node* next = node->next;
                size_type new_idx = node->hash_value % count;
                node->next = new_buckets[new_idx];
                new_buckets[new_idx] = node;
                node = next;
            }
        }
        
        m_buckets = std::move(new_buckets);
    }
    
    void reserve(size_type count) {
        if (count > max_size())
            unordered_map_detail::length_failure();
        rehash(static_cast<size_type>(count / m_max_load_factor) + 1);
    }
    
    /* ハッシュ・比較オブジェクト */
    hasher hash_function() const { return m_hash; }
    key_equal key_eq() const { return m_equal; }
    
    /* swap */
    void swap(unordered_map& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            m_alloc != other.m_alloc) {
            swap_unequal(other);
            return;
        }
        swap_allocator(other, typename allocator_traits<allocator_type>::propagate_on_container_swap());
        m_buckets.swap(other.m_buckets);
        unordered_map_detail::exchange(m_size, other.m_size);
        unordered_map_detail::exchange(m_max_load_factor, other.m_max_load_factor);
        unordered_map_detail::exchange(m_hash, other.m_hash);
        unordered_map_detail::exchange(m_equal, other.m_equal);
    }

private:
    void swap_allocator(unordered_map& other, true_type) noexcept {
        unordered_map_detail::exchange(m_alloc, other.m_alloc);
    }

    void swap_allocator(unordered_map&, false_type) noexcept {}

public:
    allocator_type get_allocator() const noexcept { return m_alloc; }
};

/* 非メンバ関数 */
template<typename K, typename T, typename H, typename E, typename A>
bool operator==(const unordered_map<K, T, H, E, A>& lhs, const unordered_map<K, T, H, E, A>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (const auto& pair : lhs) {
        auto it = rhs.find(pair.first);
        if (it == rhs.end() || it->second != pair.second) return false;
    }
    return true;
}

template<typename K, typename T, typename H, typename E, typename A>
bool operator!=(const unordered_map<K, T, H, E, A>& lhs, const unordered_map<K, T, H, E, A>& rhs) {
    return !(lhs == rhs);
}

template<typename K, typename T, typename H, typename E, typename A>
void swap(unordered_map<K, T, H, E, A>& lhs, unordered_map<K, T, H, E, A>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

/* ═══════════════════════════════════════════════════════════════
 * unordered_multimap - 同じキーで複数の値を持てるバージョン（簡易実装）
 * ═══════════════════════════════════════════════════════════════*/

template<
    typename Key,
    typename T,
    typename Hash = hash<Key>,
    typename KeyEqual = equal_to<Key>,
    typename Allocator = allocator<pair<const Key, T>>
>
class unordered_multimap {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = pair<const Key, T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;

private:
    /* シンプルなvectorベースの実装 - インプレース構築対応 */
    struct Entry {
        alignas(value_type) unsigned char storage[sizeof(value_type)];
        Entry* next;
        size_t hash_value;

        value_type& data() { return *reinterpret_cast<value_type*>(storage); }
        const value_type& data() const { return *reinterpret_cast<const value_type*>(storage); }

        template<typename K, typename V>
        Entry(K&& k, V&& v, size_t h) : next(nullptr), hash_value(h) {
            ::new (storage) value_type(std::forward<K>(k), std::forward<V>(v));
        }

        ~Entry() {
            data().~value_type();
        }
    };

    using entry_allocator_type = typename allocator_traits<allocator_type>::template
        rebind_alloc<Entry>;
    using entry_allocator_traits = allocator_traits<entry_allocator_type>;
    using entry_pointer = typename entry_allocator_traits::pointer;

public:
    class node_type {
        Entry* m_entry;
        allocator_type m_alloc;

        node_type(Entry* entry, const allocator_type& alloc)
            : m_entry(entry), m_alloc(alloc) {}
        friend class unordered_multimap;

        void reset() noexcept {
            if (!m_entry) return;
            entry_allocator_type entry_alloc(m_alloc);
            entry_allocator_traits::destroy(entry_alloc, m_entry);
            entry_allocator_traits::deallocate(
                entry_alloc,
                pointer_traits<entry_pointer>::pointer_to(*m_entry), 1);
            m_entry = nullptr;
        }

    public:
        node_type() : m_entry(nullptr), m_alloc() {}
        node_type(node_type&& other) noexcept
            : m_entry(other.m_entry), m_alloc(std::move(other.m_alloc)) {
            other.m_entry = nullptr;
        }
        node_type& operator=(node_type&& other) noexcept {
            if (this != &other) {
                reset();
                m_entry = other.m_entry;
                m_alloc = std::move(other.m_alloc);
                other.m_entry = nullptr;
            }
            return *this;
        }
        node_type(const node_type&) = delete;
        node_type& operator=(const node_type&) = delete;
        ~node_type() { reset(); }

        bool empty() const noexcept { return m_entry == nullptr; }
        explicit operator bool() const noexcept { return !empty(); }
        key_type& key() const { return const_cast<key_type&>(m_entry->data().first); }
        mapped_type& mapped() const { return m_entry->data().second; }
        allocator_type get_allocator() const noexcept { return m_alloc; }
    };

private:

    allocator_type alloc_;
    vector<Entry*> buckets_;
    size_type size_;
    float max_load_factor_;
    hasher hash_;
    key_equal equal_;

    static constexpr size_type INITIAL_BUCKET_COUNT = 16;

    template<typename... Args>
    Entry* allocate_entry(Args&&... args) {
        entry_allocator_type entry_alloc(alloc_);
        entry_pointer allocation = entry_allocator_traits::allocate(entry_alloc, 1);
        Entry* entry = unordered_map_detail::pointer_address(allocation);
        if (allocation == entry_pointer() || !entry) {
            if (allocation != entry_pointer())
                entry_allocator_traits::deallocate(entry_alloc, allocation, 1);
            unordered_map_detail::allocation_failure();
        }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
        try {
            entry_allocator_traits::construct(
                entry_alloc, entry, std::forward<Args>(args)...);
        } catch (...) {
            entry_allocator_traits::deallocate(entry_alloc, allocation, 1);
            throw;
        }
#else
        entry_allocator_traits::construct(
            entry_alloc, entry, std::forward<Args>(args)...);
#endif
        return entry;
    }

    void destroy_entry(Entry* entry) noexcept {
        entry_allocator_type entry_alloc(alloc_);
        entry_allocator_traits::destroy(entry_alloc, entry);
        entry_allocator_traits::deallocate(
            entry_alloc, pointer_traits<entry_pointer>::pointer_to(*entry), 1);
    }

    void relocate_values(unordered_multimap& source,
                         unordered_multimap& target, true_type) {
        for (const auto& value : source)
            target.insert(value);
    }

    void relocate_values(unordered_multimap& source,
                         unordered_multimap& target, false_type) {
        for (size_type index = 0; index < source.buckets_.size(); ++index) {
            for (Entry* entry = source.buckets_[index]; entry;
                 entry = entry->next) {
                target.insert(value_type(entry->data().first,
                                         std::move(entry->data().second)));
            }
        }
    }

    void swap_unequal(unordered_multimap& other) {
        if (other.size_ > max_size() || size_ > other.max_size())
            unordered_map_detail::length_failure();
        const size_type left_buckets = other.buckets_.size() ?
            other.buckets_.size() : INITIAL_BUCKET_COUNT;
        const size_type right_buckets = buckets_.size() ?
            buckets_.size() : INITIAL_BUCKET_COUNT;
        unordered_multimap left(left_buckets, other.hash_, other.equal_, alloc_);
        unordered_multimap right(right_buckets, hash_, equal_, other.alloc_);
        left.max_load_factor_ = other.max_load_factor_;
        right.max_load_factor_ = max_load_factor_;
        relocate_values(other, left, is_copy_constructible<value_type>());
        relocate_values(*this, right, is_copy_constructible<value_type>());
        swap(left);
        other.swap(right);
    }

    void copy_assign_allocator(const allocator_type& other, true_type) {
        alloc_ = other;
    }

    void copy_assign_allocator(const allocator_type&, false_type) {}

    void commit_copy_assignment(unordered_multimap& candidate, true_type) {
        hash_ = candidate.hash_;
        equal_ = candidate.equal_;
        clear();
        alloc_ = candidate.alloc_;
        buckets_ = std::move(candidate.buckets_);
        size_ = candidate.size_;
        candidate.size_ = 0;
    }

    void commit_copy_assignment(unordered_multimap& candidate, false_type) {
        swap(candidate);
    }

    void rehash_if_needed() {
        if (max_load_factor_ > 0.0f && !buckets_.empty() &&
            load_factor() > max_load_factor_) {
            rehash(buckets_.size() * 2);
        }
    }

#if __cplusplus > 202002L
    template<typename R>
    void insert_range_impl(R&& range, true_type) {
        const size_type candidate_buckets = buckets_.size() ?
            buckets_.size() : INITIAL_BUCKET_COUNT;
        unordered_multimap candidate(candidate_buckets, hash_, equal_, alloc_);
        candidate.max_load_factor_ = max_load_factor_;
        for (const auto& value : *this) candidate.insert(value);
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) candidate.insert(*first);

        clear();
        buckets_ = std::move(candidate.buckets_);
        size_ = candidate.size_;
        candidate.size_ = 0;
    }

    template<typename R>
    void insert_range_impl(R&& range, false_type) {
        const size_type candidate_buckets = buckets_.size() ?
            buckets_.size() : INITIAL_BUCKET_COUNT;
        unordered_multimap candidate(candidate_buckets, hash_, equal_, alloc_);
        candidate.max_load_factor_ = max_load_factor_;
        for (size_type index = 0; index < buckets_.size(); ++index) {
            for (Entry* entry = buckets_[index]; entry; entry = entry->next) {
                candidate.insert(value_type(entry->data().first,
                                             std::move(entry->data().second)));
            }
        }
        auto first = ranges::begin(range);
        auto last = ranges::end(range);
        for (; first != last; ++first) candidate.insert(std::move(*first));

        clear();
        buckets_ = std::move(candidate.buckets_);
        size_ = candidate.size_;
        candidate.size_ = 0;
    }
#endif

    size_type bucket_index(size_t h) const {
        return buckets_.empty() ? 0 : h % buckets_.size();
    }

    bool locate_node(const Entry* target, size_type& bucket) const {
        if (!target) return false;
        for (size_type i = 0; i < buckets_.size(); ++i) {
            for (const Entry* entry = buckets_[i]; entry; entry = entry->next) {
                if (entry == target) {
                    bucket = i;
                    return true;
                }
            }
        }
        return false;
    }

    Entry* insert_entry_from_handle(node_type& node, true_type) {
        Entry* replacement = allocate_entry(node.m_entry->data().first,
                                            node.m_entry->data().second,
                                            node.m_entry->hash_value);
        node.reset();
        return replacement;
    }

    Entry* insert_entry_from_handle(node_type& node, false_type) {
        Entry* replacement = allocate_entry(
            node.m_entry->data().first,
            std::move(node.m_entry->data().second),
            node.m_entry->hash_value);
        node.reset();
        return replacement;
    }

public:
    class const_iterator {
        friend class unordered_multimap;
        const unordered_multimap* map_;
        size_type bucket_;
        const Entry* node_;

        void advance_to_valid() {
            while (node_ == nullptr && bucket_ < map_->buckets_.size()) {
                ++bucket_;
                if (bucket_ < map_->buckets_.size()) {
                    node_ = map_->buckets_[bucket_];
                }
            }
        }

    public:
        using iterator_category = forward_iterator_tag;
        using value_type = unordered_multimap::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() : map_(nullptr), bucket_(0), node_(nullptr) {}
        const_iterator(const unordered_multimap* m, size_type b, const Entry* n)
            : map_(m), bucket_(b), node_(n) {
            if (node_ == nullptr && map_) advance_to_valid();
        }

        reference operator*() const { return node_->data(); }
        pointer operator->() const { return &node_->data(); }

        const_iterator& operator++() {
            if (node_) {
                node_ = node_->next;
                if (node_ == nullptr) {
                    ++bucket_;
                    if (bucket_ < map_->buckets_.size()) {
                        node_ = map_->buckets_[bucket_];
                    }
                    advance_to_valid();
                }
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const {
            return map_ == other.map_ && bucket_ == other.bucket_ && node_ == other.node_;
        }
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };

    using iterator = const_iterator;  /* シンプル化: const_iteratorのみ */

    unordered_multimap()
        : alloc_(), buckets_(INITIAL_BUCKET_COUNT, nullptr), size_(0),
          max_load_factor_(1.0f), hash_(), equal_() {}

    unordered_multimap(size_type bucket_count,
                       const hasher& hash,
                       const key_equal& equal,
                       const allocator_type& alloc)
        : alloc_(alloc),
          buckets_(bucket_count ? bucket_count : INITIAL_BUCKET_COUNT, nullptr),
          size_(0), max_load_factor_(1.0f), hash_(hash), equal_(equal) {}

    explicit unordered_multimap(const allocator_type& alloc)
        : unordered_multimap(INITIAL_BUCKET_COUNT, hasher(), key_equal(), alloc) {}

    template<typename InputIt,
             typename = typename std::enable_if<!std::is_integral<InputIt>::value>::type>
    unordered_multimap(InputIt first, InputIt last,
                       size_type bucket_count = INITIAL_BUCKET_COUNT,
                       const hasher& hash = hasher(),
                       const key_equal& equal = key_equal(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multimap(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    unordered_multimap(initializer_list<value_type> init,
                       size_type bucket_count = INITIAL_BUCKET_COUNT,
                       const hasher& hash = hasher(),
                       const key_equal& equal = key_equal(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multimap(bucket_count, hash, equal, alloc) {
        insert(init);
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    unordered_multimap(from_range_t, R&& range,
                       size_type bucket_count = INITIAL_BUCKET_COUNT,
                       const hasher& hash = hasher(),
                       const key_equal& equal = key_equal(),
                       const allocator_type& alloc = allocator_type())
        : unordered_multimap(bucket_count, hash, equal, alloc) {
        insert_range(std::forward<R>(range));
    }
#endif

    unordered_multimap(const unordered_multimap& other)
        : alloc_(allocator_traits<allocator_type>::select_on_container_copy_construction(other.alloc_)),
          buckets_(other.buckets_.size(), nullptr), size_(0),
          max_load_factor_(other.max_load_factor_), hash_(other.hash_), equal_(other.equal_) {
        for (const auto& value : other) insert(value);
    }

    unordered_multimap(unordered_multimap&& other) noexcept
        : alloc_(std::move(other.alloc_)), buckets_(std::move(other.buckets_)),
          size_(other.size_), max_load_factor_(other.max_load_factor_),
          hash_(std::move(other.hash_)), equal_(std::move(other.equal_)) {
        other.size_ = 0;
    }

    ~unordered_multimap() {
        clear();
    }

    iterator begin() const noexcept {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            if (buckets_[i]) return const_iterator(this, i, buckets_[i]);
        }
        return end();
    }

    iterator end() const noexcept {
        return const_iterator(this, buckets_.size(), nullptr);
    }

    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept {
        return allocator_traits<allocator_type>::max_size(alloc_);
    }

    void clear() {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            Entry* node = buckets_[i];
            while (node) {
                Entry* next = node->next;
                destroy_entry(node);
                node = next;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

    iterator insert(const value_type& value) {
        size_t h = hash_(value.first);
        if (buckets_.empty()) buckets_.resize(INITIAL_BUCKET_COUNT, nullptr);
        rehash_if_needed();
        size_type idx = bucket_index(h);

        Entry* last_equivalent = nullptr;
        for (Entry* entry = buckets_[idx]; entry; entry = entry->next) {
            if (entry->hash_value == h && equal_(entry->data().first, value.first))
                last_equivalent = entry;
        }

        Entry* new_node = allocate_entry(value.first, value.second, h);
        if (last_equivalent) {
            new_node->next = last_equivalent->next;
            last_equivalent->next = new_node;
        } else {
            new_node->next = buckets_[idx];
            buckets_[idx] = new_node;
        }
        ++size_;
        return const_iterator(this, idx, new_node);
    }

    iterator insert(value_type&& value) {
        size_t h = hash_(value.first);
        if (buckets_.empty()) buckets_.resize(INITIAL_BUCKET_COUNT, nullptr);
        rehash_if_needed();
        size_type idx = bucket_index(h);
        Entry* last_equivalent = nullptr;
        for (Entry* entry = buckets_[idx]; entry; entry = entry->next) {
            if (entry->hash_value == h && equal_(entry->data().first, value.first))
                last_equivalent = entry;
        }
        Entry* new_node = allocate_entry(std::move(value.first), std::move(value.second), h);
        if (last_equivalent) {
            new_node->next = last_equivalent->next;
            last_equivalent->next = new_node;
        } else {
            new_node->next = buckets_[idx];
            buckets_[idx] = new_node;
        }
        ++size_;
        return const_iterator(this, idx, new_node);
    }

    iterator insert(node_type&& node) {
        if (node.empty()) return end();
        const size_t h = hash_(node.m_entry->data().first);
        if (buckets_.empty()) buckets_.resize(INITIAL_BUCKET_COUNT, nullptr);
        rehash_if_needed();
        const size_type idx = bucket_index(h);
        Entry* last_equivalent = nullptr;
        for (Entry* entry = buckets_[idx]; entry; entry = entry->next) {
            if (entry->hash_value == h &&
                equal_(entry->data().first, node.m_entry->data().first))
                last_equivalent = entry;
        }
        Entry* adopted = nullptr;
        if (alloc_ == node.m_alloc) {
            adopted = node.m_entry;
            node.m_entry = nullptr;
        } else {
            adopted = insert_entry_from_handle(node,
                is_copy_constructible<value_type>());
        }
        adopted->hash_value = h;
        if (last_equivalent) {
            adopted->next = last_equivalent->next;
            last_equivalent->next = adopted;
        } else {
            adopted->next = buckets_[idx];
            buckets_[idx] = adopted;
        }
        ++size_;
        return const_iterator(this, idx, adopted);
    }

    iterator insert(const_iterator /* hint */, node_type&& node) {
        return insert(std::move(node));
    }

    node_type extract(const_iterator position) {
        if ((!position.node_ && position.map_ != this) || !position.node_)
            return node_type();
        size_type idx = 0;
        Entry* target = const_cast<Entry*>(position.node_);
        if (!locate_node(target, idx)) return node_type();
        Entry* previous = nullptr;
        Entry* current = buckets_[idx];
        while (current && current != target) {
            previous = current;
            current = current->next;
        }
        if (!current) return node_type();
        if (previous) previous->next = current->next;
        else buckets_[idx] = current->next;
        current->next = nullptr;
        --size_;
        return node_type(current, alloc_);
    }

    node_type extract(const key_type& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    node_type extract(const K& key) {
        iterator position = find(key);
        return position == end() ? node_type() : extract(position);
    }
#endif

    void merge(unordered_multimap& source) {
        if (this == &source) return;
        for (size_type index = 0; index < source.buckets_.size(); ++index) {
            Entry* current = source.buckets_[index];
            while (current) {
                Entry* next = current->next;
                node_type detached = source.extract(
                    typename unordered_multimap::const_iterator(
                        &source, index, current));
                insert(std::move(detached));
                current = next;
            }
        }
    }

    void merge(unordered_map<Key, T, Hash, KeyEqual, Allocator>& source);

    template<typename H2, typename E2, typename A2>
    void merge(unordered_map<Key, T, H2, E2, A2>& source);

    template<typename H2, typename E2, typename A2>
    void merge(unordered_multimap<Key, T, H2, E2, A2>& source);

    iterator insert(const_iterator, const value_type& value) {
        return insert(value);
    }

    iterator insert(const_iterator, value_type&& value) {
        return insert(std::move(value));
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (; first != last; ++first) insert(*first);
    }

    void insert(initializer_list<value_type> init) {
        insert(init.begin(), init.end());
    }

#if __cplusplus > 202002L
    template<typename R>
        requires detail::container_compatible_range<R, value_type>
    void insert_range(R&& range) {
        insert_range_impl(std::forward<R>(range),
                          is_copy_constructible<value_type>());
    }
#endif

    template<typename... Args>
    iterator emplace_hint(const_iterator, Args&&... args) {
        return emplace(std::forward<Args>(args)...);
    }

    size_type count(const key_type& key) const {
        if (buckets_.empty()) return 0;
        size_t h = hash_(key);
        size_type idx = bucket_index(h);
        size_type c = 0;
        for (const Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                ++c;
            }
        }
        return c;
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type count(const K& key) const {
        if (buckets_.empty()) return 0;
        const size_t h = hash_(key);
        const size_type idx = bucket_index(h);
        size_type count = 0;
        for (const Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                ++count;
            }
        }
        return count;
    }
#endif

    bool contains(const key_type& key) const {
        return count(key) != 0;
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    bool contains(const K& key) const {
        return count(key) != 0;
    }
#endif

    size_type bucket_count() const noexcept { return buckets_.size(); }
    size_type max_bucket_count() const noexcept { return max_size(); }

    size_type bucket_size(size_type n) const {
        if (n >= buckets_.size()) return 0;
        size_type count = 0;
        for (const Entry* entry = buckets_[n]; entry; entry = entry->next) ++count;
        return count;
    }

    size_type bucket(const key_type& key) const {
        return buckets_.empty() ? 0 : bucket_index(hash_(key));
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    size_type bucket(const K& key) const {
        return buckets_.empty() ? 0 : bucket_index(hash_(key));
    }
#endif

    float load_factor() const noexcept {
        return buckets_.empty() ? 0.0f : static_cast<float>(size_) / buckets_.size();
    }

    float max_load_factor() const noexcept { return max_load_factor_; }
    void max_load_factor(float value) {
        if (!(value > 0.0f))
            unordered_map_detail::load_factor_failure();
        max_load_factor_ = value;
    }

    void rehash(size_type count) {
        if (count < INITIAL_BUCKET_COUNT) count = INITIAL_BUCKET_COUNT;
        if (max_load_factor_ > 0.0f &&
            count < static_cast<size_type>(size_ / max_load_factor_)) {
            count = static_cast<size_type>(size_ / max_load_factor_);
        }
        if (count > max_bucket_count())
            unordered_map_detail::length_failure();
        if (count == buckets_.size()) return;
        vector<Entry*> replacement(count, nullptr);
        for (size_type i = 0; i < buckets_.size(); ++i) {
            Entry* entry = buckets_[i];
            while (entry) {
                Entry* next = entry->next;
                const size_type index = entry->hash_value % count;
                entry->next = replacement[index];
                replacement[index] = entry;
                entry = next;
            }
        }
        buckets_ = std::move(replacement);
    }

    void reserve(size_type count) {
        if (count > max_size())
            unordered_map_detail::length_failure();
        if (max_load_factor_ > 0.0f)
            rehash(static_cast<size_type>(count / max_load_factor_) + 1);
        else
            rehash(count);
    }

    hasher hash_function() const { return hash_; }
    key_equal key_eq() const { return equal_; }

    /* emplace - construct element in-place */
    template<typename... Args>
    iterator emplace(Args&&... args) {
        /* Create a temporary pair to extract the key */
        value_type tmp(std::forward<Args>(args)...);
        return insert(std::move(tmp));
    }

    /* equal_range - return range of elements with key */
    pair<iterator, iterator> equal_range(const key_type& key) {
        if (buckets_.empty()) return make_pair(end(), end());
        size_t h = hash_(key);
        size_type idx = bucket_index(h);
        iterator first = end();
        iterator last = end();
        bool found_first = false;

        for (Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                if (!found_first) {
                    first = const_iterator(this, idx, node);
                    found_first = true;
                }
                /* Keep advancing to find the last one */
                last = const_iterator(this, idx, node);
                ++last;
            }
        }
        if (!found_first) {
            return make_pair(end(), end());
        }
        return make_pair(first, last);
    }

    pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        return const_cast<unordered_multimap*>(this)->equal_range(key);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<iterator, iterator> equal_range(const K& key) {
        if (buckets_.empty()) return make_pair(end(), end());
        const size_t h = hash_(key);
        const size_type idx = bucket_index(h);
        iterator first = end();
        iterator last = end();
        bool found_first = false;
        for (Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                if (!found_first) {
                    first = const_iterator(this, idx, node);
                    found_first = true;
                }
                last = const_iterator(this, idx, node);
                ++last;
            }
        }
        if (!found_first) return make_pair(end(), end());
        return make_pair(first, last);
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return const_cast<unordered_multimap*>(this)->equal_range(key);
    }
#endif

    /* erase by iterator */
    iterator erase(const_iterator pos) {
        if (!pos.node_ && pos.map_ != this) return end();
        size_type idx = 0;
        Entry* prev = nullptr;
        Entry* node = const_cast<Entry*>(pos.node_);
        if (!locate_node(node, idx)) return end();
        node = buckets_[idx];

        while (node && node != pos.node_) {
            prev = node;
            node = node->next;
        }

        if (!node) return end();

        if (prev) {
            prev->next = node->next;
        } else {
            buckets_[idx] = node->next;
        }
        destroy_entry(node);
        --size_;
        if (buckets_[idx]) return const_iterator(this, idx, buckets_[idx]);
        for (size_type next = idx + 1; next < buckets_.size(); ++next)
            if (buckets_[next]) return const_iterator(this, next, buckets_[next]);
        return end();
    }

    /* erase range */
    iterator erase(const_iterator first, const_iterator last) {
        if ((!first.node_ && first.map_ != this) ||
            (!last.node_ && last.map_ != this)) return end();
        if (first.node_ && !locate_node(first.node_, first.bucket_)) return end();
        if (last.node_ && !locate_node(last.node_, last.bucket_)) return end();
        const_iterator boundary(this, last.bucket_, last.node_);
        while (first.node_ != boundary.node_) first = erase(first);
        return const_iterator(this, boundary.bucket_, boundary.node_);
    }

    /* erase by key */
    size_type erase(const key_type& key) {
        size_type removed = 0;
        auto range = equal_range(key);
        while (range.first != range.second) {
            range.first = erase(range.first);
            ++removed;
        }
        return removed;
    }

    /* find */
    iterator find(const key_type& key) {
        if (buckets_.empty()) return end();
        size_t h = hash_(key);
        size_type idx = bucket_index(h);
        for (Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                return const_iterator(this, idx, node);
            }
        }
        return end();
    }

    const_iterator find(const key_type& key) const {
        return const_cast<unordered_multimap*>(this)->find(key);
    }

#if __cplusplus >= 202002L
    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    iterator find(const K& key) {
        if (buckets_.empty()) return end();
        const size_t h = hash_(key);
        const size_type idx = bucket_index(h);
        for (Entry* node = buckets_[idx]; node; node = node->next) {
            if (node->hash_value == h && equal_(node->data().first, key)) {
                return const_iterator(this, idx, node);
            }
        }
        return end();
    }

    template<typename K,
             typename = enable_if_t<
                 unordered_map_detail::heterogeneous_lookup<
                     hasher, key_equal, key_type, decay_t<K>>::value>>
    const_iterator find(const K& key) const {
        return const_cast<unordered_multimap*>(this)->find(key);
    }
#endif

    unordered_multimap& operator=(const unordered_multimap& other) {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_copy_assignment;
            unordered_multimap candidate(
                other.buckets_.size(), other.hash_, other.equal_,
                propagate::value ? other.alloc_ : alloc_);
            candidate.max_load_factor_ = other.max_load_factor_;
            for (const auto& value : other) candidate.insert(value);
            commit_copy_assignment(candidate, propagate());
        }
        return *this;
    }

    unordered_multimap& operator=(unordered_multimap&& other) noexcept {
        if (this != &other) {
            using propagate = typename allocator_traits<allocator_type>::
                propagate_on_container_move_assignment;
            if (propagate::value || alloc_ == other.alloc_) {
                clear();
                if (propagate::value) alloc_ = std::move(other.alloc_);
                buckets_ = std::move(other.buckets_);
                size_ = other.size_;
                max_load_factor_ = other.max_load_factor_;
                hash_ = std::move(other.hash_);
                equal_ = std::move(other.equal_);
                other.size_ = 0;
            } else {
                clear();
                buckets_.resize(other.buckets_.size(), nullptr);
                for (size_type i = 0; i < buckets_.size(); ++i) buckets_[i] = nullptr;
                hash_ = other.hash_;
                equal_ = other.equal_;
                max_load_factor_ = other.max_load_factor_;
                for (size_type index = 0; index < other.buckets_.size(); ++index) {
                    for (Entry* entry = other.buckets_[index]; entry;
                         entry = entry->next) {
                        insert(value_type(entry->data().first,
                                          std::move(entry->data().second)));
                    }
                }
                other.clear();
            }
        }
        return *this;
    }

    void swap(unordered_multimap& other) {
        if (this == &other) return;
        if (!allocator_traits<allocator_type>::propagate_on_container_swap::value &&
            alloc_ != other.alloc_) {
            swap_unequal(other);
            return;
        }
        swap_allocator(other,
            typename allocator_traits<allocator_type>::propagate_on_container_swap());
        buckets_.swap(other.buckets_);
        unordered_map_detail::exchange(size_, other.size_);
        unordered_map_detail::exchange(max_load_factor_, other.max_load_factor_);
        unordered_map_detail::exchange(hash_, other.hash_);
        unordered_map_detail::exchange(equal_, other.equal_);
    }

private:
    void swap_allocator(unordered_multimap& other, true_type) noexcept {
        unordered_map_detail::exchange(alloc_, other.alloc_);
    }

    void swap_allocator(unordered_multimap&, false_type) noexcept {}

public:
    allocator_type get_allocator() const noexcept { return alloc_; }
};

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
void unordered_map<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& source) {
    if (reinterpret_cast<void*>(this) == reinterpret_cast<void*>(&source)) return;
    typedef unordered_multimap<Key, T, Hash, KeyEqual, Allocator> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(it->first) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(node.key(), std::move(node.mapped()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
void unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_map<Key, T, Hash, KeyEqual, Allocator>& source) {
    if (reinterpret_cast<void*>(this) == reinterpret_cast<void*>(&source)) return;
    typedef unordered_map<Key, T, Hash, KeyEqual, Allocator> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(node.key(), std::move(node.mapped()));
        insert(std::move(value));
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
template<typename H2, typename E2, typename A2>
void unordered_map<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_map<Key, T, H2, E2, A2>& source) {
    typedef unordered_map<Key, T, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(it->first) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(node.key(), std::move(node.mapped()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
template<typename H2, typename E2, typename A2>
void unordered_map<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_multimap<Key, T, H2, E2, A2>& source) {
    typedef unordered_multimap<Key, T, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        if (find(it->first) == end()) {
            typename source_type::node_type node = source.extract(it);
            value_type value(node.key(), std::move(node.mapped()));
            insert(std::move(value));
        }
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
template<typename H2, typename E2, typename A2>
void unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_map<Key, T, H2, E2, A2>& source) {
    typedef unordered_map<Key, T, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(node.key(), std::move(node.mapped()));
        insert(std::move(value));
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
template<typename H2, typename E2, typename A2>
void unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::merge(
    unordered_multimap<Key, T, H2, E2, A2>& source) {
    typedef unordered_multimap<Key, T, H2, E2, A2> source_type;
    for (typename source_type::iterator it = source.begin();
         it != source.end();) {
        typename source_type::iterator next = it;
        ++next;
        typename source_type::node_type node = source.extract(it);
        value_type value(node.key(), std::move(node.mapped()));
        insert(std::move(value));
        it = next;
    }
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
bool operator==(const unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& lhs,
                const unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (const auto& value : lhs) {
        auto lhs_range = lhs.equal_range(value.first);
        auto rhs_range = rhs.equal_range(value.first);
        typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::size_type
            lhs_matches = 0;
        typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::size_type
            rhs_matches = 0;
        for (auto it = lhs_range.first; it != lhs_range.second; ++it) {
            if (it->second == value.second) ++lhs_matches;
        }
        for (auto it = rhs_range.first; it != rhs_range.second; ++it) {
            if (it->second == value.second) ++rhs_matches;
        }
        if (lhs_matches != rhs_matches) return false;
    }
    return true;
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
bool operator!=(const unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& lhs,
                const unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& rhs) {
    return !(lhs == rhs);
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
void swap(unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& lhs,
          unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& rhs)
    noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

#if __cplusplus >= 202002L
template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
typename unordered_map<Key, T, Hash, KeyEqual, Allocator>::size_type
erase(unordered_map<Key, T, Hash, KeyEqual, Allocator>& value,
      const Key& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator, typename Pred>
typename unordered_map<Key, T, Hash, KeyEqual, Allocator>::size_type
erase_if(unordered_map<Key, T, Hash, KeyEqual, Allocator>& value,
         Pred predicate) {
    typename unordered_map<Key, T, Hash, KeyEqual, Allocator>::size_type removed = 0u;
    for (typename unordered_map<Key, T, Hash, KeyEqual, Allocator>::iterator it =
             value.begin(); it != value.end();) {
        if (predicate(*it)) {
            it = value.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator>
typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::size_type
erase(unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& value,
      const Key& key) {
    return value.erase(key);
}

template<typename Key, typename T, typename Hash, typename KeyEqual,
         typename Allocator, typename Pred>
typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::size_type
erase_if(unordered_multimap<Key, T, Hash, KeyEqual, Allocator>& value,
         Pred predicate) {
    typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::size_type removed = 0u;
    for (typename unordered_multimap<Key, T, Hash, KeyEqual, Allocator>::iterator it =
             value.begin(); it != value.end();) {
        if (predicate(*it)) {
            it = value.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}
#endif

} /* namespace std */

#endif /* __cplusplus */
#endif /* RINCXX_UNORDERED_MAP_H */
